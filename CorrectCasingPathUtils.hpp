#pragma once

#include <filesystem>

/*
    Important! Absolute path only.
    MUST NOT SPECIFY relative path or UNC or short file name.
*/
std::filesystem::path correct_path_casing(const std::filesystem::path&file_path);