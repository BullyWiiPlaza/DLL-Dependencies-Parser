#pragma once

#include <filesystem>

class resolved_dll_dependencies
{
	public:
		std::vector<std::string> dll_load_failures;

		std::vector<std::string> missing_dlls;
		
		std::vector<std::string> referenced_dlls;
};

class dll_references_resolver
{
	std::filesystem::path resolve_absolute_dll_file_path(const std::filesystem::path& module_name) const;

	void add_module_file_paths(const std::filesystem::path& parsed_module_file_path) const;

	public:
	    std::filesystem::path executable_file_path;

	    std::filesystem::path results_output_file_path;

	    bool skip_parsing_windows_dll_dependencies;

		resolved_dll_dependencies resolve_references() const;
};
