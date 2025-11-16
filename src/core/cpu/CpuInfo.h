#pragma once
#include "../common/PlatformDefs.h"
#include <string>
#include <queue>
#include <vector>
#include <memory>

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    // Windows已经定义了ULONG、DWORD、WORD、BOOL
#elif defined(PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <CoreFoundation/CoreFoundation.h>
    // ULONG is already defined by macOS headers as UInt32
    typedef unsigned long DWORD;
    typedef unsigned short WORD;
    typedef int BOOL;
#elif defined(PLATFORM_LINUX)
    #include <fstream>
    #include <sstream>
    #include <unistd.h>
    typedef unsigned long long ULONG;
    typedef unsigned long DWORD;
    typedef unsigned short WORD;
    typedef int BOOL;
#endif

// 跨平台通用类型定义（仅在Windows平台未定义时）
#ifndef PLATFORM_WINDOWS
    typedef unsigned short WORD;
    typedef int BOOL;
#endif

#ifdef PLATFORM_WINDOWS
    #define PDH_CSTATUS_VALID_DATA 0x00000000L
    #define PDH_CSTATUS_NEW_DATA 0x00000001L
    #ifndef ERROR_SUCCESS
        #define ERROR_SUCCESS 0
    #endif
#else
    typedef void* HANDLE;
    typedef void* PDH_HQUERY;
    typedef void* PDH_HCOUNTER;
    typedef long PDH_STATUS;
    #define PDH_CSTATUS_VALID_DATA 0x00000000L
    #define PDH_CSTATUS_NEW_DATA 0x00000001L
    #define ERROR_SUCCESS 0
#endif

// 原有CpuInfo类保持完全不变，确保向后兼容
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

    // 新增：获取最近一次 CPU 使用率采样间隔（毫秒）
    double GetLastSampleIntervalMs() const { return lastSampleIntervalMs; }

    // 新增：CPU 基准/即时频率（MHz）
    double GetBaseFrequencyMHz() const;     
    double GetCurrentFrequencyMHz() const;  

private:
    void DetectCores();
    void InitializeCounter();
    void InitializeFrequencyCounter();
    void CleanupCounter();
    void CleanupFrequencyCounter();
    void UpdateCoreSpeeds();             // 新增：更新核心频率
    std::string GetNameFromRegistry();
    std::string GetNameFromSysctl();  // macOS
    std::string GetNameFromProcCpuinfo(); // Linux
    double updateUsage();
    double updateInstantFrequencyMHz();

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

    // 平台特定的成员变量
    #if defined(PLATFORM_WINDOWS) || (defined(_WIN32) || defined(_WIN64))
    // Windows PDH 计数器相关（使用率）
    PDH_HQUERY queryHandle;
    PDH_HCOUNTER counterHandle;
    bool counterInitialized;

    // PDH 计数器相关（频率 MHz）
    PDH_HQUERY freqQueryHandle;
    PDH_HCOUNTER freqCounterHandle;
    bool freqCounterInitialized;
    DWORD lastFreqTick;
    double cachedInstantMHz;
    #else
    // macOS/Linux 特有成员
    void* queryHandle;
    void* counterHandle;
    bool counterInitialized;
    void* freqQueryHandle;
    void* freqCounterHandle;
    bool freqCounterInitialized;
    DWORD lastFreqTick;
    double cachedInstantMHz;
    #endif
};

// 新增：跨平台CPU适配器接口
class ICpuAdapter {
public:
    virtual ~ICpuAdapter() = default;
    virtual double GetUsage() = 0;
    virtual std::string GetName() = 0;
    virtual int GetTotalCores() const = 0;
    virtual int GetSmallCores() const = 0;
    virtual int GetLargeCores() const = 0;
    virtual double GetLargeCoreSpeed() const = 0;
    virtual double GetSmallCoreSpeed() const = 0;
    virtual DWORD GetCurrentSpeed() const = 0;
    virtual bool IsHyperThreadingEnabled() const = 0;
    virtual bool IsVirtualizationEnabled() const = 0;
    virtual double GetLastSampleIntervalMs() const = 0;
    virtual double GetBaseFrequencyMHz() const = 0;
    virtual double GetCurrentFrequencyMHz() const = 0;
    virtual bool Initialize() = 0;
    virtual void Cleanup() = 0;
    virtual bool Update() = 0;
};

// Windows平台适配器：包装原有CpuInfo类
class WinCpuAdapter : public ICpuAdapter {
public:
    WinCpuAdapter();
    virtual ~WinCpuAdapter();

    virtual double GetUsage() override;
    virtual std::string GetName() override;
    virtual int GetTotalCores() const override;
    virtual int GetSmallCores() const override;
    virtual int GetLargeCores() const override;
    virtual double GetLargeCoreSpeed() const override;
    virtual double GetSmallCoreSpeed() const override;
    virtual DWORD GetCurrentSpeed() const override;
    virtual bool IsHyperThreadingEnabled() const override;
    virtual bool IsVirtualizationEnabled() const override;
    virtual double GetLastSampleIntervalMs() const override;
    virtual double GetBaseFrequencyMHz() const override;
    virtual double GetCurrentFrequencyMHz() const override;
    virtual bool Initialize() override;
    virtual void Cleanup() override;
    virtual bool Update() override;

private:
    std::unique_ptr<CpuInfo> m_originalCpuInfo;
};
