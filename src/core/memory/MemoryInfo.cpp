#include "MemoryInfo.h"
#include <windows.h>

MemoryInfo::MemoryInfo() {
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
}

ULONGLONG MemoryInfo::GetTotalPhysical() const {
    return memStatus.ullTotalPhys;
}

ULONGLONG MemoryInfo::GetAvailablePhysical() const {
    return memStatus.ullAvailPhys;
}

ULONGLONG MemoryInfo::GetTotalVirtual() const {
    return memStatus.ullTotalVirtual;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           