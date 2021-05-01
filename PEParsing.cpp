#include <iostream>
#include <unordered_set>
#include <parser-library/parse.h>
#include <CLI/CLI.hpp>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string/predicate.hpp>

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
    const std::string &module_name, const std::string &symbol_name)
{
    (void) output_buffer;
    (void) virtual_address;
    (void) symbol_name;
	
    module_file_paths.insert(module_name);

	// Continue iterating
    return 0;
}

inline std::filesystem::path resolve_absolute_dll_file_path(const std::filesystem::path& module_name)
{
	if (const auto module_handle = LoadLibrary(module_name.string().c_str());
        module_handle != nullptr)
    {
        char module_file_path[MAX_PATH];
        GetModuleFileNameA(module_handle, module_file_path, MAX_PATH);
		
        if (const auto freeing_succeeded = FreeLibrary(module_handle);
            !freeing_succeeded)
        {
            std::cerr << "FreeLibrary() failed on " << module_name << "..." << std::endl;
        }
        return module_file_path;
    }
    
    return "";
}

std::set<std::filesystem::path> parsed_module_file_paths;

inline auto add_module_names(const std::filesystem::path &pe_file_path)
{
    spdlog::info("Parsing PE " + pe_file_path.string() + "...");
    const auto parsed_pe = open_executable(pe_file_path);
    if (!parsed_pe)
    {
        throw std::runtime_error("Failed parsing PE file " + pe_file_path.string());
    }

    spdlog::info("Dumping imported module names...");
    IterImpVAString(parsed_pe.get(), &dump_module_names, nullptr);

    std::set<std::filesystem::path> updated_module_names;
    for(auto &module_name : module_file_paths)
    {
	    if (const auto absolute_module_name = resolve_absolute_dll_file_path(module_name);
            !absolute_module_name.empty())
    	{
            updated_module_names.insert(absolute_module_name);
        }
        else
        {
            updated_module_names.insert(module_name);
        }
    }

    module_file_paths = updated_module_names;
	
    spdlog::info("Module name count: " + std::to_string(module_file_paths.size()));
    parsed_module_file_paths.insert(pe_file_path);
}

inline std::string bool_to_string(const bool value)
{
    return value ? "true" : "false";
}

inline std::filesystem::path get_system_32_directory()
{
    TCHAR file_path[MAX_PATH];
    GetSystemDirectoryA(file_path, MAX_PATH);
    return file_path;
}

int main(const int argument_count, char* arguments[])
{	
    try
    {
        spdlog::set_level(spdlog::level::level_enum::debug);
        spdlog::info("Referenced DLL Parser (C) 2021 BullyWiiPlaza Productions");
    	
        CLI::App application{"Referenced DLL Parser"};

        std::filesystem::path pe_file_path;
        application.add_option("--pe-file-path", pe_file_path, "The file path to the PE file")
        ->required()
    	->check(CLI::ExistingFile);
        auto resolve_dlls = false;
        application.add_flag("--resolve-dlls", resolve_dlls, "Whether to resolve all dependent DLLs");
        auto skip_parsing_system32_dll_dependencies = false;
        application.add_flag("--skip-parsing-system32-dll-dependencies", skip_parsing_system32_dll_dependencies, "Whether system32 DLLs will not be parsed (for performance gains)");
        std::filesystem::path results_output_file_path;
        application.add_flag("--results-output-file-path", results_output_file_path, "The file to write the results to")
            ->check(CLI::ExistingFile);
    	
    	CLI11_PARSE(application, argument_count, arguments)

        spdlog::info("PE file path: " + pe_file_path.string());
        spdlog::info("Resolve DLLs: " + bool_to_string(resolve_dlls));
    	
    	// Set the current directory to the executable's parent directory
		const auto parent_file_path = pe_file_path.parent_path().string();
        SetCurrentDirectory(parent_file_path.c_str());

    	// Begin the modules iteration with the executable
        module_file_paths.insert(pe_file_path.string());

        const auto system_32_directory = get_system_32_directory();
    	
        spdlog::info("Finding dependent DLLs recursively...");
        const execution_timer timer;
        while (true)
        {
            const auto previous_module_file_path_count = module_file_paths.size();
            for (int module_file_path_index = 0; module_file_path_index < previous_module_file_path_count; module_file_path_index++)
            {
                auto iterator = module_file_paths.begin();
                std::advance(iterator, module_file_path_index);
                const auto& module_file_path = *iterator;
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
            	
                add_module_names(module_file_path);
            }

            if (const auto updated_module_file_path_count = module_file_paths.size();
                updated_module_file_path_count == previous_module_file_path_count)
        	{
                spdlog::debug("No more new modules found...");
                break;
        	}
        }

        const auto message = timer.build_log_message("Finding dependent DLLs");
        spdlog::debug(message);
    	
        if (resolve_dlls)
        {
            std::set<std::filesystem::path> dll_load_failures;
            for (const auto& module_file_path : module_file_paths)
            {
                if (resolve_absolute_dll_file_path(module_file_path.filename()).empty())
                {
                    dll_load_failures.insert(module_file_path);
                }
            }
            
            if (dll_load_failures.empty())
            {
                spdlog::info("No DLL load failures detected");
            }
        	else
            {
                spdlog::info("DLL load failures:");
                for (const auto& dll_load_failure : dll_load_failures)
                {
                    spdlog::info(dll_load_failure.string());
                }
            }
        }
        else
        {
            spdlog::info("Recursively referenced DLLs:");
            for (const auto& module_file_path : module_file_paths)
            {
                spdlog::info(module_file_path.string());
            }
        }

        return EXIT_SUCCESS;
    }
	catch (std::exception &exception)
	{
        spdlog::error(exception.what());
        return EXIT_FAILURE;
	}
}