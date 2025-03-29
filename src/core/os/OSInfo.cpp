#include "OSInfo.h"
#include "WinUtils.h"
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif


#pragma comment(lib, "ntdll.lib")

extern "C" NTSYSAPI NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW lpVersionInformation);

OSInfo::OSInfo() {
    RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
    if (RtlGetVersion(&osvi) == STATUS_SUCCESS) {
        osVersion = WinUtils::WstringToString(
            std::wstring(L"Windows ") + 
            std::to_wstring(osvi.dwMajorVersion) + L"." + 
            std::to_wstring(osvi.dwMinorVersion) + 
            L" (Build " + std::to_wstring(osvi.dwBuildNumber) + L")"
        );
    } else {
        osVersion = "Unknown OS Version";
    }
}

std::string OSInfo::GetVersion() const {
    return osVersion;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         