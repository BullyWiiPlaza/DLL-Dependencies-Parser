#include "DLLReferencesResolver.hpp"

#include <pe-parse/parse.h>
#include <CLI/CLI.hpp>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string/predicate.hpp>
#include "CorrectCasingPathUtils.hpp"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "UserProfileEnvironmentUtils.hpp"
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

inline std::filesystem::path get_windows_directory()
{
    char file_path[MAX_PATH]{};
    if (const auto length_copied = GetWindowsDirectoryA(file_path, MAX_PATH);
        length_copied == 0)
    {
        throw std::runtime_error("GetWindowsDirectory() failed");
    }
    return file_path;
}

std::filesystem::path dll_references_resolver::resolve_absolute_dll_file_path(const std::filesystem::path& module_name) const
{
    if (const auto module_handle = LoadLibraryA(module_name.string().c_str());
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

        if (const auto windows_directory = get_windows_directory();
            !boost::istarts_with(module_file_path, windows_directory.string())
            && !boost::istarts_with(module_file_path, executable_file_path.parent_path().string()))
        {
            return "";
        }

        return module_file_path;
    }

    return "";
}

void dll_references_resolver::add_module_file_paths(const std::filesystem::path& parsed_module_file_path)
{
    parsed_module_file_paths_.insert(parsed_module_file_path);

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
            const auto corrected_casing = correct_path_casing(absolute_module_file_path.string());
            updated_module_file_paths.insert(corrected_casing);
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

inline auto write_to_file(const std::string& file_contents, const std::filesystem::path& file_path)
{
    std::ofstream file_writer(file_path, std::ios::binary);
    if (file_writer.fail())
    {
        throw std::runtime_error("Failed writing to " + file_path.string());
    }
    file_writer << file_contents;
}

resolved_dll_dependencies dll_references_resolver::resolve_references()
{
    parsed_module_file_paths_.clear();
    module_file_paths.clear();

    if (!is_regular_file(executable_file_path))
    {
        throw std::runtime_error("Input file \"" + executable_file_path.string() + "\" does not exist");
    }

    // Set the current directory to the executable's parent directory
    const auto parent_file_path = executable_file_path.parent_path().string();
    SetCurrentDirectoryA(parent_file_path.c_str());

    // Begin the modules iteration with the executable
    module_file_paths.insert(executable_file_path.string());

    std::set<std::filesystem::path> missing_dlls_file_names;

    const auto windows_directory = get_windows_directory();

    spdlog::info("Finding dependent DLLs recursively...");
    const execution_timer timer;
    while (true)
    {
        std::set<std::filesystem::path> copied_module_file_paths = module_file_paths;
        const auto previous_module_file_path_count = module_file_paths.size();
        for (auto& module_file_path : copied_module_file_paths)
        {
            if (skip_parsing_windows_dll_dependencies
                && boost::istarts_with(module_file_path.string(), windows_directory.string()))
            {
                spdlog::debug("Skipping to parse Windows directory module " + module_file_path.string() + "...");
                continue;
            }

            if (parsed_module_file_paths_.contains(module_file_path))
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

        if (resolve_absolute_dll_file_path(module_file_path.filename()).empty()
            && !boost::iends_with(module_file_path.string(), ".exe"))
        {
            dll_load_failures.insert(module_file_path);
        }
    }

    json missing_dlls_json = json::array();
    std::vector<std::string> missing_dlls_vector;
    for (const auto& missing_dlls_file_name : missing_dlls_file_names)
    {
        const auto missing_dll_file_path = replace_user_profile_with_environment_variable(missing_dlls_file_name).string();
        missing_dlls_json.push_back(missing_dll_file_path);
        missing_dlls_vector.push_back(missing_dll_file_path);
    }
    output_json["missing-dlls"] = missing_dlls_json;

    json dll_load_failures_json = json::array();
    std::vector<std::string> dll_load_failures_vector;
    for (const auto& dll_load_failure : dll_load_failures)
    {
        const auto dll_load_failure_file_path = replace_user_profile_with_environment_variable(dll_load_failure).string();
        dll_load_failures_json.push_back(dll_load_failure_file_path);
        dll_load_failures_vector.push_back(dll_load_failure_file_path);
    }
    output_json["dll-load-failures"] = dll_load_failures_json;

    json referenced_dlls_json = json::array();
    std::vector<std::string> referenced_dlls_vector;
    // Exclude the PE file again
    module_file_paths.erase(executable_file_path);
    for (const auto& module_file_path : module_file_paths)
    {
    	if (boost::iends_with(module_file_path.string(), ".exe"))
    	{
            continue;
    	}
    	
        const auto modified_module_file_path = replace_user_profile_with_environment_variable(module_file_path).string();
        referenced_dlls_json.push_back(modified_module_file_path);
        referenced_dlls_vector.push_back(modified_module_file_path);
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

    return { dll_load_failures_vector, missing_dlls_vector, referenced_dlls_vector };
}