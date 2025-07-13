#include "OSInfo.h"
#include "../utils/WinUtils.h"
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// 使用动态加载方式获取RtlGetVersion函数
typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

OSInfo::OSInfo() {
    RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
    
    // 动态获取RtlGetVersion函数指针
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
        if (RtlGetVersion && RtlGetVersion(&osvi) == STATUS_SUCCESS) {
            osVersion = WinUtils::WstringToString(
                std::wstring(L"Windows ") + 
                std::to_wstring(osvi.dwMajorVersion) + L"." + 
                std::to_wstring(osvi.dwMinorVersion) + 
                L" (Build " + std::to_wstring(osvi.dwBuildNumber) + L")"
            );
        } else {
            osVersion = "Unknown OS Version";
        }
    } else {
        osVersion = "Unknown OS Version";
    }
}

std::string OSInfo::GetVersion() const {
    return osVersion;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         