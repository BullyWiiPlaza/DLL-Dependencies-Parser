#pragma once

#include <string>
#include <filesystem>

std::string get_environment_variable(const std::string& environment_variable_name);

std::filesystem::path replace_user_profile_with_environment_variable(const std::filesystem::path& file_path);