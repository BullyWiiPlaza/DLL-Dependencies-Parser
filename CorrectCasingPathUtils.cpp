#include <filesystem>
#include <stdexcept>
#include <spdlog/details/backtracer.h>

#include "CorrectCasingPathUtils.hpp"

inline std::string get_properly_capitalized_file_name(const std::filesystem::path& file_path)
{
    WIN32_FIND_DATAA find_data{};
    const auto find_file_handle = FindFirstFileA(file_path.string().c_str(), &find_data);
    if (find_file_handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("FindFirstFile() failed");
    }

    FindClose(find_file_handle);

    const auto parent_file_path = file_path.parent_path();
    return find_data.cFileName;
}

std::filesystem::path correct_path_casing(const std::filesystem::path& file_path)
{
    if (!exists(file_path))
    {
        throw std::runtime_error("File path " + file_path.string() + " does not exist");
    }

    std::filesystem::path updated_file_path;
    updated_file_path += static_cast<char>(toupper(file_path.c_str()[0]));
    updated_file_path += ":\\";

    std::vector<std::string> file_path_components;

    std::filesystem::path iteration_file_path = file_path;
    while (true)
    {
        const auto properly_capitalized_file_name = get_properly_capitalized_file_name(iteration_file_path);
        file_path_components.push_back(properly_capitalized_file_name);

        iteration_file_path = iteration_file_path.parent_path();
        // File system root detected
        if (iteration_file_path == updated_file_path.string())
        {
            break;
        }
    }

    for (int file_path_component_index = static_cast<int>(file_path_components.size()) - 1; file_path_component_index >= 0; file_path_component_index--)
    {
        updated_file_path /= file_path_components.at(file_path_component_index);
    }

    return updated_file_path;
}