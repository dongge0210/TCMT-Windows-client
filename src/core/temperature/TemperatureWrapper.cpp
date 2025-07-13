#include "TemperatureWrapper.h"

// 切换到托管代码模式来调用LibreHardwareMonitorBridge
#pragma managed
#include "../Utils/LibreHardwareMonitorBridge.h"

// 静态成员定义
bool TemperatureWrapper::initialized = false;

void TemperatureWrapper::Initialize() {
    try {
        LibreHardwareMonitorBridge::Initialize();
        initialized = true;
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
}

std::vector<std::pair<std::string, double>> TemperatureWrapper::GetTemperatures() {
    if (!initialized) {
        return std::vector<std::pair<std::string, double>>();
    }
    
    try {
        return LibreHardwareMonitorBridge::GetTemperatures();
    }
    catch (...) {
        return std::vector<std::pair<std::string, double>>();
    }
}

bool TemperatureWrapper::IsInitialized() {
    return initialized;
}
