#pragma once
#include "../factory/InfoFactory.h"
#include <memory>

// 前向声明原有CpuInfo类，避免包含Windows特定头文件
class CpuInfo;

/*
 * CPU信息兼容性适配器
 * 
 * 这个类提供了原有CpuInfo接口的兼容性包装，
 * 同时支持跨平台扩展。使用策略模式在运行时选择实现：
 * - 默认使用原有的Windows实现（保持100%兼容性）
 * - 可选择使用跨平台适配器（支持未来扩展）
 */
class CpuInfoCompat {
public:
    // 使用模式枚举
    enum class ImplementationMode {
        Legacy,     // 使用原有CpuInfo类（默认）
        Adapter     // 使用跨平台适配器
    };

    CpuInfoCompat(ImplementationMode mode = ImplementationMode::Legacy);
    ~CpuInfoCompat();

    // 原有CpuInfo接口 - 完全兼容
    double GetUsage();
    std::string GetName();
    int GetTotalCores() const;
    int GetSmallCores() const;
    int GetLargeCores() const;
    double GetLargeCoreSpeed() const;
    double GetSmallCoreSpeed() const;
    uint32_t GetCurrentSpeed() const;
    bool IsHyperThreadingEnabled() const;
    bool IsVirtualizationEnabled() const;
    double GetLastSampleIntervalMs() const;
    double GetBaseFrequencyMHz() const;
    double GetCurrentFrequencyMHz() const;

    // 跨平台扩展方法
    bool SwitchToAdapter();
    bool SwitchToLegacy();
    ImplementationMode GetCurrentMode() const;
    bool IsAdapterAvailable() const;
    
    // 错误处理
    std::string GetLastError() const;
    void ClearError();

private:
    ImplementationMode m_mode;
    std::unique_ptr<CpuInfo> m_legacyCpuInfo;
    std::unique_ptr<ICpuAdapter> m_adapterCpuInfo;
    
    void Initialize();
    void Cleanup();
    void SetError(const std::string& error);
    std::string m_lastError;
};

// 便利宏定义，用于快速切换实现
#define CPU_INFO_USE_ADAPTER() CpuInfoCompat compat(CpuInfoCompat::ImplementationMode::Adapter)
#define CPU_INFO_USE_LEGACY() CpuInfoCompat compat(CpuInfoCompat::ImplementationMode::Legacy)