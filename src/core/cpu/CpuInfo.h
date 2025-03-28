#pragma once
#include <string>
#include <windows.h>

class CpuInfo {
public:
    CpuInfo();
    double GetUsage();
    std::string GetName();
    int GetTotalCores() const;
    int GetSmallCores() const;
    int GetLargeCores() const;
    DWORD GetCurrentSpeed() const;
    bool IsHyperThreadingEnabled() const;
    bool IsVirtualizationEnabled() const;

private:
    void DetectCores();
    std::string GetNameFromRegistry();
    double updateUsage();
    double cpuUsage = 0.0;
    std::string cpuName;
    int totalCores = 0;
    int smallCores = 0;
    int largeCores = 0;
    DWORD cpuCurrentSpeed = 0;
    ULONGLONG lastTotalSysTime = 0;
    ULONGLONG lastTotalIdleTime = 0;
};