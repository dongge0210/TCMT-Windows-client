#include "TemperatureWrapper.h"
#include "../gpu/GpuInfo.h"
#include "../utils/Logger.h"
#include "../utils/WmiManager.h"
#include <algorithm>
#include <cwctype>
// 切换到托管代码模式来调用LibreHardwareMonitorBridge
#pragma managed
#include "../Utils/LibreHardwareMonitorBridge.h"

// 静态成员定义
bool TemperatureWrapper::initialized = false;
static GpuInfo* gpuInfo = nullptr;
static WmiManager* wmiManager = nullptr;

void TemperatureWrapper::Initialize() {
    try {
        LibreHardwareMonitorBridge::Initialize();
        initialized = true;
        if (!wmiManager) wmiManager = new WmiManager();
        if (!gpuInfo && wmiManager && wmiManager->IsInitialized()) {
            gpuInfo = new GpuInfo(*wmiManager);
        }
    }
    catch (...) {
        initialized = false;
        throw;
    }
}

void TemperatureWrapper::Cleanup() {
    if (initialized) {
        LibreHardwareMonitorBridge::Cleanup();
        initialized = false;
    }
    if (gpuInfo) { delete gpuInfo; gpuInfo = nullptr; }
    if (wmiManager) { delete wmiManager; wmiManager = nullptr; }
}

std::vector<std::pair<std::string, double>> TemperatureWrapper::GetTemperatures() {
    std::vector<std::pair<std::string, double>> temps;
    
    // 1. 先获取libre的
    if (initialized) {
        try {
            auto libreTemps = LibreHardwareMonitorBridge::GetTemperatures();
            temps.insert(temps.end(), libreTemps.begin(), libreTemps.end());
        } catch (...) {
            // 静默失败
        }
    }
    
    // 2. 再获取GpuInfo的（过滤虚拟GPU）
    if (gpuInfo) {
        const auto& gpus = gpuInfo->GetGpuData();
        for (const auto& gpu : gpus) {
            if (gpu.isVirtual) continue;
            std::string gpuName(gpu.name.begin(), gpu.name.end());
            temps.emplace_back("GPU: " + gpuName, static_cast<double>(gpu.temperature));
        }
    }
    
    return temps;
}

bool TemperatureWrapper::IsInitialized() {
    return initialized;
}