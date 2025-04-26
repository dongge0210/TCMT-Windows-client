#include "stdafx.h"
#include "WinUtils.h"
#include <windows.h>
#include <string>
#include <sstream>

#pragma comment(lib, "advapi32.lib") // Required for security-related functions like OpenProcessToken

#define WINUTILS_IMPLEMENTED 1

bool WinUtils::EnablePrivilege(const std::wstring& privilegeName, bool enable /*= true*/) {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }
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
    CloseHandle(token);
    return (GetLastError() == ERROR_SUCCESS);
}

bool WinUtils::CheckPrivilege(const std::wstring& privilegeName) {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValue(NULL, privilegeName.c_str(), &luid)) {
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
    PSID adminGroupSid = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    // Create SID for the Administrators group
    if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroupSid)) {
        return false;
    }
    
    // Check if the current user is a member of the Administrators group
    if (!CheckTokenMembership(NULL, adminGroupSid, &isAdmin)) {
        isAdmin = FALSE;
    }
    
    FreeSid(adminGroupSid);
    return (isAdmin != FALSE);
}

std::string WinUtils::FormatWindowsErrorMessage(DWORD errorCode) {
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );
    if (!buffer) {
        return "Unknown error: " + std::to_string(errorCode);
    }
    std::wstring wideMsg(buffer);
    LocalFree(buffer);
    return std::string(wideMsg.begin(), wideMsg.end());
}