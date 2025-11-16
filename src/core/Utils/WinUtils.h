#pragma once
#include <string>

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef PLATFORM_WINDOWS
class WinUtils {
public:
    // UTF-16 -> UTF-8（使用 -1，让 WideCharToMultiByte 自动包含终止符）
    static std::string WstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, &result[0], size, nullptr, nullptr);
        return result;
    }

    // UTF-8 -> UTF-16
    static std::wstring Utf8ToWstring(const std::string& str) {
        if (str.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, nullptr, 0);
        std::wstring result(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, &result[0], size);
        return result;
    }

    // 获取当前进程ID
    static DWORD GetCurrentProcessId() {
        return ::GetCurrentProcessId();
    }

    // 检查特权
    static bool CheckPrivilege(const std::wstring& privilegeName);
    static bool EnablePrivilege(const std::wstring& privilegeName, bool enable = true);
    static bool IsRunAsAdmin();
    static std::string FormatWindowsErrorMessage(DWORD errorCode);
    static std::string GetExecutableDirectory();
    
    // 字符串转换函数
    static std::string WstringToString(const std::wstring& wstr);
    static std::wstring StringToWstring(const std::string& str);
};
#endif