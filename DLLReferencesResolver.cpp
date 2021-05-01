#include "DLLReferencesResolver.hpp"

#include <parser-library/parse.h>
#include <CLI/CLI.hpp>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string/predicate.hpp>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "ExecutionTimer.hpp"

using parsed_pe_ref = std::unique_ptr<peparse::parsed_pe, void (*)(peparse::parsed_pe*)>;

inline auto open_executable(const std::filesystem::path& path)
{
    parsed_pe_ref parsed_pe(peparse::ParsePEFromFile(path.string().data()), peparse::DestructParsedPE);
    if (!parsed_pe)
    {
        return parsed_pe_ref(nullptr, peparse::DestructParsedPE);
    }

    return parsed_pe;
}

std::set<std::filesystem::path> module_file_paths;

inline auto dump_module_names(void* output_buffer, const peparse::VA& virtual_address,
    const std::string& module_name, const std::string& symbol_name)
{
    (void)output_buffer;
    (void)virtual_address;
    (void)symbol_name;

    module_file_paths.insert(module_name);

    // Continue iterating
    return 0;
}

inline std::filesystem::path resolve_absolute_dll_file_path(const std::filesystem::path& module_name)
{
    const execution_timer timer;
    if (const auto module_handle = LoadLibrary(module_name.string().c_str());
        module_handle != nullptr)
    {
        char module_file_path[MAX_PATH];
        GetModuleFileNameA(module_handle, module_file_path, MAX_PATH);

        // Freeing the library is expensive so don't do it
        /* if (const auto freeing_succeeded = FreeLibrary(module_handle);
            !freeing_succeeded)
        {
            std::cerr << "FreeLibrary() failed on " << module_name << "..." << std::endl;
        } */
        return module_file_path;
    }

    return "";
}

std::set<std::filesystem::path> parsed_module_file_paths;

inline auto add_module_file_paths(const std::filesystem::path& parsed_module_file_path)
{
    parsed_module_file_paths.insert(parsed_module_file_path);

    const execution_timer timer;
    spdlog::debug("Parsing PE file " + parsed_module_file_path.string() + "...");
    const auto parsed_pe = open_executable(parsed_module_file_path);
    if (!parsed_pe)
    {
        throw std::runtime_error("Failed parsing PE file " + parsed_module_file_path.string());
    }

    spdlog::debug("Dumping imported module names...");
    IterImpVAString(parsed_pe.get(), &dump_module_names, nullptr);

    std::set<std::filesystem::path> updated_module_file_paths;
    for (auto& module_name : module_file_paths)
    {
        if (const auto absolute_module_file_path = resolve_absolute_dll_file_path(module_name);
            !absolute_module_file_path.empty())
        {
            updated_module_file_paths.insert(absolute_module_file_path);
        }
        else
        {
            updated_module_file_paths.insert(module_name);
        }
    }

    module_file_paths = updated_module_file_paths;

    spdlog::debug("Module name count: " + std::to_string(module_file_paths.size()));
    const auto timer_log_message = timer.build_log_message("Getting imported modules for " + parsed_module_file_path.string());
    spdlog::debug(timer_log_message);
}

inline std::filesystem::path get_system_32_directory()
{
    TCHAR file_path[MAX_PATH];
    GetSystemDirectoryA(file_path, MAX_PATH);
    return file_path;
}

inline auto write_to_file(const std::string& file_contents, const std::filesystem::path& file_path)
{
    std::ofstream file_writer(file_path, std::ios::binary);
    if (file_writer.fail())
    {
        throw std::runtime_error("Failed writing to " + file_path.string());
    }
    file_writer << file_contents;
}

void dll_references_resolver::resolve_references() const
{
    // Set the current directory to the executable's parent directory
    const auto parent_file_path = executable_file_path.parent_path().string();
    SetCurrentDirectory(parent_file_path.c_str());

    // Begin the modules iteration with the executable
    module_file_paths.insert(executable_file_path.string());

    std::set<std::filesystem::path> missing_dlls_file_names;
    
    const auto system_32_directory = get_system_32_directory();

    spdlog::info("Finding dependent DLLs recursively...");
    const execution_timer timer;
    while (true)
    {
        auto copied_module_file_paths = module_file_paths;
        const auto previous_module_file_path_count = module_file_paths.size();
        for (auto& module_file_path : copied_module_file_paths)
        {
            if (skip_parsing_system32_dll_dependencies && boost::istarts_with(module_file_path.string(), system_32_directory.string()))
            {
                spdlog::debug("Skipping to parse system32 directory module...");
                continue;
            }

            if (parsed_module_file_paths.contains(module_file_path))
            {
                spdlog::debug("Module " + module_file_path.string() + " already parsed, skipping...");
                continue;
            }

            try
            {
                add_module_file_paths(module_file_path);
            }
            catch (std::exception& exception)
            {
                spdlog::error(exception.what());
                missing_dlls_file_names.insert(module_file_path);
            }
        }

        if (const auto updated_module_file_path_count = module_file_paths.size();
            updated_module_file_path_count == previous_module_file_path_count)
        {
            spdlog::debug("No more new modules found...");
            break;
        }
    }
    
    json output_json;

    std::set<std::filesystem::path> dll_load_failures;
    for (const auto& module_file_path : module_file_paths)
    {
        if (missing_dlls_file_names.contains(module_file_path))
        {
            continue;
        }
    	
        if (resolve_absolute_dll_file_path(module_file_path.filename()).empty())
        {
            dll_load_failures.insert(module_file_path);
        }
    }

    json missing_dlls_json = json::array();
    for (const auto& missing_dlls_file_name : missing_dlls_file_names)
    {
        missing_dlls_json.push_back(missing_dlls_file_name.string());
    }
    output_json["missing-dlls"] = missing_dlls_json;
	
    json dll_load_failures_json = json::array();
    for (const auto& dll_load_failure : dll_load_failures)
    {
        dll_load_failures_json.push_back(dll_load_failure.string());
    }
    output_json["dll-load-failures"] = dll_load_failures_json;

    json referenced_dlls_json = json::array();
    for (const auto& module_file_path : module_file_paths)
    {
    	// Exclude EXEs etc.
    	if (boost::iends_with(module_file_path.string(), "dll"))
    	{
            referenced_dlls_json.push_back(module_file_path.string());
    	}
    }
    output_json["referenced-dlls"] = referenced_dlls_json;

    const auto printed_output_json = output_json.dump(4);
    spdlog::info("Result JSON:\n" + printed_output_json);

    if (!results_output_file_path.empty())
    {
        spdlog::info("Writing result JSON to " + results_output_file_path.string() + "...");
        write_to_file(printed_output_json, results_output_file_path);
    }

    const auto message = timer.build_log_message("DLL references resolver");
    spdlog::info(message);
}