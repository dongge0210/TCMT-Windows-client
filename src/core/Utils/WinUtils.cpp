#include "WinUtils.h"
#include <codecvt>
#include <locale>
#include <cstdarg>

std::wstring WinUtils::Utf8ToWide(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}

std::string WinUtils::WideToUtf8(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

std::string WinUtils::FormatWstring(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);

    wchar_t buffer[1024];
    vswprintf_s(buffer, format, args);

    va_end(args);
    return WideToUtf8(buffer);
}

std::string WinUtils::WstringToString(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0],
        static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);

    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()),
        &str[0], size_needed, NULL, NULL);

    return str;
}

std::wstring WinUtils::StringToWstring(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0],
        static_cast<int>(str.size()), NULL, 0);

    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()),
        &wstr[0], size_needed);

    return wstr;
}