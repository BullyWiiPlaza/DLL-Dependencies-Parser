#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include "DLLReferencesResolver.hpp"

inline std::string bool_to_string(const bool value)
{
    return value ? "true" : "false";
}

int main(const int argument_count, char* arguments[])
{	
    try
    {
#if _DEBUG
        spdlog::set_level(spdlog::level::level_enum::debug);
#endif
    	
        spdlog::info("Referenced DLL Parser (C) 2021 BullyWiiPlaza Productions");
    	
        CLI::App application{"Referenced DLL Parser"};

        std::filesystem::path pe_file_path;
        application.add_option("--pe-file-path", pe_file_path, "The file path to the PE file")
        ->required()
    	->check(CLI::ExistingFile);
        auto skip_parsing_system32_dll_dependencies = false;
        application.add_flag("--skip-parsing-system32-dll-dependencies", skip_parsing_system32_dll_dependencies, "Whether system32 DLLs will not be parsed (for performance gains)");
        std::filesystem::path results_output_file_path;
        application.add_option("--results-output-file-path", results_output_file_path, "The file to write the results to");
    	
    	CLI11_PARSE(application, argument_count, arguments)

        spdlog::info("PE file path: " + pe_file_path.string());
        spdlog::info("Skip parsing system32 DLL dependencies: " + bool_to_string(skip_parsing_system32_dll_dependencies));
        results_output_file_path = absolute(results_output_file_path);
        spdlog::info("Results output file path: " + results_output_file_path.string());
        
        dll_references_resolver references_resolver;
        references_resolver.pe_file_path = pe_file_path;
        references_resolver.skip_parsing_system32_dll_dependencies = skip_parsing_system32_dll_dependencies;
        references_resolver.results_output_file_path = results_output_file_path;
        references_resolver.resolve_references();

        return EXIT_SUCCESS;
    }
	catch (std::exception &exception)
	{
        spdlog::error(exception.what());
        return EXIT_FAILURE;
	}
}