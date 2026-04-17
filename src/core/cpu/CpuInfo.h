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
    double GetLargeCoreSpeed() const;    // 性能核心频率
    double GetSmallCoreSpeed() const;    // 能效核心频率
    uint32_t GetCurrentSpeed() const;    // 保持兼容性
    bool IsHyperThreadingEnabled() const;
    bool IsVirtualizationEnabled() const;

    // 最近一次 CPU 使用率采样间隔（毫秒）
    double GetLastSampleIntervalMs() const { return lastSampleIntervalMs; }

private:
    void DetectCores();
    void InitializeCounter();
    void CleanupCounter();
    void UpdateCoreSpeeds();
    std::string GetNameFromRegistry();
    double updateUsage();

    // 基本信息
    std::string cpuName;
    int totalCores;
    int smallCores;
    int largeCores;
    double cpuUsage;

    // 频率信息
    double largeCoreSpeed;
    double smallCoreSpeed;
    double lastSampleIntervalMs;

#ifdef TCMT_WINDOWS
    void* queryHandle;       // PDH_HQUERY
    void* counterHandle;     // PDH_HCOUNTER
    bool counterInitialized;
    uint32_t lastUpdateTime;
    uint32_t lastSampleTick;
    uint32_t prevSampleTick;
#endif

#ifdef TCMT_MACOS
    uint64_t prevTotalTicks;
    uint64_t prevIdleTicks;
    uint64_t prevSampleTimeMs;
    // P-core / E-core 分组频率
    std::vector<double> pCoreSpeeds;
    std::vector<double> eCoreSpeeds;
#endif
};
