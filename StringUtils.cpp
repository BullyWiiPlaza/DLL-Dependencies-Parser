#include "StringUtils.hpp"

#include <stdexcept>
#include <Windows.h>

std::string wide_string_to_string(const std::wstring& wide_string)
{
    if (wide_string.empty())
    {
        return {};
    }

    const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(),
        static_cast<int>(wide_string.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
    }

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_string.data(),
        static_cast<int>(wide_string.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

std::wstring string_to_wide_string(const std::string& string)
{
    if (string.empty())
    {
        return {};
    }

    const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(),
        static_cast<int>(string.size()), nullptr, 0);
    if (size_needed <= 0)
    {
        throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
    }

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, string.data(),
        static_cast<int>(string.size()), result.data(), size_needed);
    return result;
}