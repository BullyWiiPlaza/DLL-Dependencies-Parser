#include "UserProfileEnvironmentUtils.hpp"

#include <spdlog/spdlog.h>

std::string get_environment_variable(const std::string& environment_variable_name)
{
    char* buffer = nullptr;
    size_t buffer_size = 0;
    if (const auto errno_value = _dupenv_s(&buffer, &buffer_size, environment_variable_name.c_str());
        errno_value != 0)
    {
        throw std::runtime_error("_dupenv_s() failed: " + std::to_string(errno_value));
    }

    if (buffer == nullptr)
    {
        throw std::runtime_error("buffer was nullptr");
    }

    std::string environment_variable = buffer;
    free(buffer);
    return environment_variable;
}

// https://stackoverflow.com/a/3418285/3764804
inline auto replace_all(std::string& input, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }

    size_t start_position = 0;
    while ((start_position = input.find(from, start_position)) != std::string::npos)
    {
        input.replace(start_position, from.length(), to);
        start_position += to.length();
    }
}

std::filesystem::path replace_user_profile_with_environment_variable(const std::filesystem::path& file_path)
{
    // Replace the user profile part in the file path with the environment variable if applicable
    const auto user_profile_environment_variable = "USERPROFILE";
    if (const auto user_home_profile = get_environment_variable(user_profile_environment_variable);
        file_path.string().starts_with(user_home_profile))
    {
        spdlog::info("Replacing user file path with environment variable...");
        auto executable_file_path_string = file_path.string();
        replace_all(executable_file_path_string, user_home_profile, std::string("%") + user_profile_environment_variable + "%");
        std::filesystem::path updated_file_path = executable_file_path_string;
        spdlog::info("Updated file path: " + updated_file_path.string());
        return updated_file_path;
    }

    return file_path;
}