#pragma once
#include <string>
#include <windows.h>
#include <pdh.h>
#include <queue>

class CpuInfo {
public:
    CpuInfo();
    ~CpuInfo();  // 添加析构函数声明

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
    void InitializeCounter();    // 新增
    void CleanupCounter();       // 新增
    std::string GetNameFromRegistry();
    double updateUsage();

    // 基本信息
    std::string cpuName;
    int totalCores;
    int smallCores;
    int largeCores;
    double cpuUsage;

    // PDH 计数器相关
    PDH_HQUERY queryHandle;
    PDH_HCOUNTER counterHandle;
    bool counterInitialized;
};
