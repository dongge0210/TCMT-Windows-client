#pragma once
#include <windows.h>
#include <string>
#include <comdef.h>

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

class WinUtils {
public:
    // UTF-8 ↔ UTF-16 转换
    static std::wstring Utf8ToWide(const std::string& str);
    static std::string WideToUtf8(const std::wstring& wstr);

    // 字符串格式化
    static std::string FormatWstring(const wchar_t* format, ...);

    // 添加WstringToString方法
	static std::string WstringToString(const std::wstring& wstr);
};
