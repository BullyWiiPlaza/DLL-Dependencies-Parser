#pragma once

#include <filesystem>

class dll_references_resolver
{
	public:
	    std::filesystem::path pe_file_path;

	    std::filesystem::path results_output_file_path;

	    bool skip_parsing_system32_dll_dependencies;

	    void resolve_references() const;
};
