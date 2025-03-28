#include "OSInfo.h"
#include "WinUtils.h"
#include <windows.h>
#include <winternl.h>

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