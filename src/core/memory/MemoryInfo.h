#pragma once
#include <windows.h>

class MemoryInfo {
public:
    MemoryInfo();
    ULONGLONG GetTotalPhysical() const;
    ULONGLONG GetAvailablePhysical() const;
    ULONGLONG GetTotalVirtual() const;

private:
    MEMORYSTATUSEX memStatus;
};                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               