#pragma once
#include <string>
#include <vector>
#include <cstdint>

// 平台宏检测（如果未在CMake中定义）
#if !defined(TCMT_WINDOWS) && !defined(TCMT_MACOS) && !defined(TCMT_LINUX)
    #if defined(_WIN32) || defined(_WIN64)
        #define TCMT_WINDOWS
    #elif defined(__APPLE__) && defined(__MACH__)
        #define TCMT_MACOS
    #elif defined(__linux__)
        #define TCMT_LINUX
    #endif
#endif

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
    uint32_t GetCurrentSpeed() const;       // 保持兼容性
    bool IsHyperThreadingEnabled() const;
    bool IsVirtualizationEnabled() const;

    // 新增：获取最近一次 CPU 使用率采样间隔（毫秒）
    double GetLastSampleIntervalMs() const { return lastSampleIntervalMs; }

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
    DWORD lastUpdateTime;                // 上次更新时间（频率）

    // 采样延迟追踪
    DWORD lastSampleTick = 0;            // 上次成功采样 Tick
    DWORD prevSampleTick = 0;            // 上一次之前的 Tick
    double lastSampleIntervalMs = 0.0;   // 最近一次采样间隔(毫秒)

    // PDH 计数器相关
    PDH_HQUERY queryHandle;
    PDH_HCOUNTER counterHandle;
    bool counterInitialized;
};
