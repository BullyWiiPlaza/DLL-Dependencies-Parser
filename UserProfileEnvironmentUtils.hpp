#pragma once

#include <string>
#include <filesystem>

std::wstring get_environment_variable(const std::wstring& environment_variable_name);

std::filesystem::path replace_user_profile_with_environment_variable(const std::filesystem::path& file_path);