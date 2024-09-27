#include "DLLReferencesResolver.hpp"

#include <pe-parse/parse.h>
#include <CLI/CLI.hpp>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string/predicate.hpp>
#include "CorrectCasingPathUtils.hpp"
#include <nlohmann/json.hpp>

#include "StringUtils.hpp"
using json = nlohmann::json;

#include "UserProfileEnvironmentUtils.hpp"
#include "ExecutionTimer.hpp"

using parsed_pe_ref = std::unique_ptr<peparse::parsed_pe, void (*)(peparse::parsed_pe*)>;

bool load_file_to_buffer(const std::filesystem::path& file_path, std::vector<uint8_t>& buffer)
{
    // Open the file in binary mode
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        spdlog::error("Failed to open file: " + wide_string_to_string(file_path.wstring()));
        return false;
    }

    // Get the file size
    const std::ifstream::pos_type file_size = file.tellg();
    if (file_size <= 0)
    {
        spdlog::error("File is empty or error in determining file size: " + wide_string_to_string(file_path.wstring()));
        return false;
    }

    // Resize the buffer to fit the file content
    buffer.resize(file_size);

    // Seek to the beginning and read the file into the buffer
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    // Check if the read operation was successful
    if (!file)
    {
        spdlog::error("Failed to read file: " + wide_string_to_string(file_path.wstring()));
        buffer.clear();
        return false;
    }

    return true;
}

inline parsed_pe_ref open_executable(const std::filesystem::path& path)
{
    std::vector<uint8_t> buffer;
    if (!load_file_to_buffer(path, buffer))
    {
        return { nullptr, peparse::DestructParsedPE };
    }

    const auto pe = peparse::ParsePEFromPointer(buffer.data(), static_cast<uint32_t>(buffer.size()));
    return { pe, peparse::DestructParsedPE };
}

std::set<std::filesystem::path> module_file_paths;

// ReSharper disable once CppParameterMayBeConstPtrOrRef
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
    wchar_t file_path[MAX_PATH];
    if (const auto length_copied = GetWindowsDirectory(file_path, MAX_PATH);
        length_copied == 0)
    {
        throw std::runtime_error("GetWindowsDirectory() failed");
    }
    return file_path;
}

std::filesystem::path dll_references_resolver::resolve_absolute_dll_file_path(const std::filesystem::path& module_name) const
{
    if (const auto module_handle = LoadLibrary(module_name.wstring().c_str());
        module_handle != nullptr)
    {
        wchar_t module_file_path[MAX_PATH];
        GetModuleFileName(module_handle, module_file_path, MAX_PATH);

        // Freeing the library is expensive so don't do it
        /* if (const auto freeing_succeeded = FreeLibrary(module_handle);
            !freeing_succeeded)
        {
            std::cerr << "FreeLibrary() failed on " << module_name << "..." << std::endl;
        } */

        if (const auto windows_directory = get_windows_directory();
            !boost::istarts_with(module_file_path, windows_directory.wstring())
            && !boost::istarts_with(module_file_path, executable_file_path.parent_path().wstring()))
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
    spdlog::debug("Parsing PE file " + wide_string_to_string(parsed_module_file_path.wstring()) + "...");
    const auto parsed_pe = open_executable(parsed_module_file_path);
    if (!parsed_pe)
    {
        throw std::runtime_error("Failed parsing PE file " + wide_string_to_string(parsed_module_file_path.wstring()));
    }

    spdlog::debug("Dumping imported module names...");
    IterImpVAString(parsed_pe.get(), &dump_module_names, nullptr);

    std::set<std::filesystem::path> updated_module_file_paths;
    for (auto& module_name : module_file_paths)
    {
        if (const auto absolute_module_file_path = resolve_absolute_dll_file_path(module_name);
            !absolute_module_file_path.empty())
        {
            const auto corrected_casing = correct_path_casing(absolute_module_file_path.wstring());
            updated_module_file_paths.insert(corrected_casing);
        }
        else
        {
            updated_module_file_paths.insert(module_name);
        }
    }

    module_file_paths = updated_module_file_paths;

    spdlog::debug("Module name count: " + std::to_string(module_file_paths.size()));
    const auto timer_log_message = timer.build_log_message("Getting imported modules for " + wide_string_to_string(parsed_module_file_path.wstring()));
    spdlog::debug(timer_log_message);
}

inline auto write_to_file(const std::string& file_contents, const std::filesystem::path& file_path)
{
    std::ofstream file_writer(file_path, std::ios::binary);
    if (file_writer.fail())
    {
        throw std::runtime_error("Failed writing to " + wide_string_to_string(file_path.wstring()));
    }
    file_writer << file_contents;
}

resolved_dll_dependencies dll_references_resolver::resolve_references()
{
    parsed_module_file_paths_.clear();
    module_file_paths.clear();

    if (!is_regular_file(executable_file_path))
    {
        throw std::runtime_error("Input file \"" + wide_string_to_string(executable_file_path.wstring()) + "\" does not exist");
    }

    // Set the current directory to the executable's parent directory
    const auto parent_file_path = executable_file_path.parent_path().wstring();
    SetCurrentDirectory(parent_file_path.c_str());

    // Begin the modules iteration with the executable
    module_file_paths.insert(executable_file_path.wstring());

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
                && boost::istarts_with(module_file_path.wstring(), windows_directory.wstring()))
            {
                spdlog::debug("Skipping to parse Windows directory module " + wide_string_to_string(module_file_path.wstring()) + "...");
                continue;
            }

            if (parsed_module_file_paths_.contains(module_file_path))
            {
                spdlog::debug("Module " + wide_string_to_string(module_file_path.wstring()) + " already parsed, skipping...");
                continue;
            }

            try
            {
                add_module_file_paths(module_file_path);
            }
            catch (const std::exception& exception)
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
            && !boost::iends_with(module_file_path.wstring(), L".exe"))
        {
            dll_load_failures.insert(module_file_path);
        }
    }

    json missing_dlls_json = json::array();
    std::vector<std::wstring> missing_dlls_vector;
    for (const auto& missing_dlls_file_name : missing_dlls_file_names)
    {
        const auto missing_dll_file_path = replace_user_profile_with_environment_variable(missing_dlls_file_name).wstring();
        missing_dlls_json.push_back(wide_string_to_string(missing_dll_file_path));
        missing_dlls_vector.push_back(missing_dll_file_path);
    }
    output_json["missing-dlls"] = missing_dlls_json;

    json dll_load_failures_json = json::array();
    std::vector<std::wstring> dll_load_failures_vector;
    for (const auto& dll_load_failure : dll_load_failures)
    {
        const auto dll_load_failure_file_path = replace_user_profile_with_environment_variable(dll_load_failure).wstring();
        dll_load_failures_json.push_back(wide_string_to_string(dll_load_failure_file_path));
        dll_load_failures_vector.push_back(dll_load_failure_file_path);
    }
    output_json["dll-load-failures"] = dll_load_failures_json;

    json referenced_dlls_json = json::array();
    std::vector<std::wstring> referenced_dlls_vector;
    // Exclude the PE file again
    module_file_paths.erase(executable_file_path);
    for (const auto& module_file_path : module_file_paths)
    {
    	if (boost::iends_with(module_file_path.wstring(), L".exe"))
    	{
            continue;
    	}
    	
        const auto modified_module_file_path = replace_user_profile_with_environment_variable(module_file_path).wstring();
        referenced_dlls_json.push_back(wide_string_to_string(modified_module_file_path));
        referenced_dlls_vector.push_back(modified_module_file_path);
    }
    output_json["referenced-dlls"] = referenced_dlls_json;

    const auto printed_output_json = output_json.dump(4);
    spdlog::info("Result JSON:\n" + printed_output_json);

    if (!results_output_file_path.empty())
    {
        spdlog::info("Writing result JSON to " + wide_string_to_string(results_output_file_path.wstring()) + "...");
        write_to_file(printed_output_json, results_output_file_path);
    }

    const auto message = timer.build_log_message("DLL references resolver");
    spdlog::info(message);

    return { dll_load_failures_vector, missing_dlls_vector, referenced_dlls_vector };
}