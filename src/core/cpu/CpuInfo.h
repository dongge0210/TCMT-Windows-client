#pragma once
#include <string>
#include <windows.h>
#include <pdh.h>
#include <queue>
#include <vector>

class CpuInfo {
public:
    CpuInfo();
    ~CpuInfo();

    double GetUsage();
    std::string GetName();
    int GetTotalCores() const;
    int GetSmallCores() const;
    int GetLargeCores() const;
    double GetLargeCoreSpeed() const;    // 新增：获取性能核心频率
    double GetSmallCoreSpeed() const;    // 新增：获取能效核心频率
    DWORD GetCurrentSpeed() const;       // 保持兼容性
    bool IsHyperThreadingEnabled() const;
    bool IsVirtualizationEnabled() const;

private:
    void DetectCores();
    void InitializeCounter();
    void CleanupCounter();
    void UpdateCoreSpeeds();             // 新增：更新核心频率
    std::string GetNameFromRegistry();
    double updateUsage();

    // 基本信息
    std::string cpuName;
    int totalCores;
    int smallCores;
    int largeCores;
    double cpuUsage;

    // 频率信息
    std::vector<DWORD> largeCoresSpeeds; // 性能核心频率
    std::vector<DWORD> smallCoresSpeeds; // 能效核心频率
    DWORD lastUpdateTime;                // 上次更新时间

    // PDH 计数器相关
    PDH_HQUERY queryHandle;
    PDH_HCOUNTER counterHandle;
    bool counterInitialized;
};
