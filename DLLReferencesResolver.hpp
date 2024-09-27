#pragma once

#include <filesystem>
#include <set>

class resolved_dll_dependencies
{
	public:
		std::vector<std::wstring> dll_load_failures;

		std::vector<std::wstring> missing_dlls;
		
		std::vector<std::wstring> referenced_dlls;
};

constexpr auto default_skip_parsing_windows_dll_dependencies = false;

class dll_references_resolver
{
	[[nodiscard]] std::filesystem::path resolve_absolute_dll_file_path(const std::filesystem::path& module_name) const;

	void add_module_file_paths(const std::filesystem::path& parsed_module_file_path);

	std::set<std::filesystem::path> parsed_module_file_paths_;

	public:
	    std::filesystem::path executable_file_path;

	    std::filesystem::path results_output_file_path;

	    bool skip_parsing_windows_dll_dependencies = default_skip_parsing_windows_dll_dependencies;

		resolved_dll_dependencies resolve_references();
};