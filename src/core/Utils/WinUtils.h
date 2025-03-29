#pragma once
#include <windows.h>
#include <string>
#include <comdef.h>

class WinUtils {
public:
    static std::wstring Utf8ToWide(const std::string& str);
    static std::string WideToUtf8(const std::wstring& wstr);
    static std::string FormatWstring(const wchar_t* format, ...);
    static std::string WstringToString(const std::wstring& wstr);
    static std::wstring StringToWstring(const std::string& str);
};
