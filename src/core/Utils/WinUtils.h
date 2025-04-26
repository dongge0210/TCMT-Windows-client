#pragma once
#include <windows.h>
#include <string>

// Utility class for Windows-specific operations
class WinUtils {
public:
    // Convert std::string to std::wstring
    static std::wstring StringToWstring(const std::string& str) {
        if (str.empty()) return std::wstring();
        
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
            static_cast<int>(str.size()), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
            &wstr[0], size_needed);
        return wstr;
    }
    
    // Convert std::wstring to std::string
    static std::string WstringToString(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
            static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
            &str[0], size_needed, NULL, NULL);
        return str;
    }
    
    // Enable a privilege by name (such as "SeCreateGlobalPrivilege")
    static bool EnablePrivilege(const std::wstring& privilegeName, bool enable = true);
    
    // Check if the current process has a specific privilege enabled
    static bool CheckPrivilege(const std::wstring& privilegeName);
    
    // Check if the current process is running with administrator privileges
    static bool IsRunAsAdmin();
    
    // Format Windows error code to message string
    static std::string FormatWindowsErrorMessage(DWORD errorCode);
};
