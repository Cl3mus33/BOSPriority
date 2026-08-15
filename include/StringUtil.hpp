#pragma once

#include <cwctype>
#include <string>

// WIN32_LEAN_AND_MEAN is already set project-wide (see top-level CMakeLists.txt).
#include <windows.h>

// Minimal UTF-8 <-> UTF-16 helpers (Win32-based, no extra dependency needed for this).
namespace StringUtil {

inline auto utf8ToUtf16(const std::string& str) -> std::wstring
{
    if (str.empty()) {
        return {};
    }

    const int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (len <= 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), len);
    return result;
}

inline auto utf16ToUtf8(const std::wstring& str) -> std::string
{
    if (str.empty()) {
        return {};
    }

    const int len = WideCharToMultiByte(
        CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), len, nullptr, nullptr);
    return result;
}

inline auto toLowerW(std::wstring s) -> std::wstring
{
    for (auto& c : s) {
        c = static_cast<wchar_t>(towlower(c));
    }
    return s;
}

} // namespace StringUtil
