#include "stdafx.h"
#include "WinUtils.h"
#include <windows.h>
#include <string>
#include <sstream>

#pragma comment(lib, "advapi32.lib")

bool WinUtils::EnablePrivilege(const std::wstring& privilegeName, bool enable) {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    LUID luid;
    if (!LookupPrivilegeValueW(nullptr, privilegeName.c_str(), &luid)) {
        CloseHandle(token);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr)) {
        CloseHandle(token);
        return false;
    }
    DWORD gle = GetLastError();
    CloseHandle(token);
    return gle == ERROR_SUCCESS;
}

bool WinUtils::CheckPrivilege(const std::wstring& privilegeName) {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return false;

    LUID luid;
    if (!LookupPrivilegeValue(nullptr, privilegeName.c_str(), &luid)) {
        CloseHandle(hToken);
        return false;
    }

    PRIVILEGE_SET ps;
    ps.PrivilegeCount = 1;
    ps.Control = PRIVILEGE_SET_ALL_NECESSARY;
    ps.Privilege[0].Luid = luid;
    ps.Privilege[0].Attributes = 0;

    BOOL result = FALSE;
    BOOL checkResult = PrivilegeCheck(hToken, &ps, &result);

    CloseHandle(hToken);
    return (checkResult && result);
}

bool WinUtils::IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroupSid = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (!AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0,0,0,0,0,0, &adminGroupSid))
        return false;

    if (!CheckTokenMembership(nullptr, adminGroupSid, &isAdmin))
        isAdmin = FALSE;

    FreeSid(adminGroupSid);
    return isAdmin != FALSE;
}

std::string WinUtils::FormatWindowsErrorMessage(DWORD errorCode) {
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );
    if (!buffer)
        return "Unknown error: " + std::to_string(errorCode);

    std::wstring wideMsg(buffer);
    LocalFree(buffer);

    // 去掉末尾的 CR/LF
    while (!wideMsg.empty() && (wideMsg.back() == L'\r' || wideMsg.back() == L'\n'))
        wideMsg.pop_back();

    return WstringToUtf8(wideMsg);
}

std::string WinUtils::GetExecutableDirectory() {
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return {};
    std::string fullPath(path, len);
    size_t pos = fullPath.find_last_of('\\');
    if (pos != std::string::npos)
        return fullPath.substr(0, pos);
    return fullPath;
}

std::string WinUtils::WstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring WinUtils::StringToWstring(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_ACP, 0, str.data(), -1, nullptr, 0);
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.data(), -1, &result[0], size);
    return result;
}