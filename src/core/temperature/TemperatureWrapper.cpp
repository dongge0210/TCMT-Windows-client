#include "TemperatureWrapper.h"
#include "../gpu/GpuInfo.h"
#include "../utils/Logger.h"

#ifdef TCMT_WINDOWS
// ======================== Windows Implementation ========================
// TemperatureWrapper uses managed C++/CLI LibreHardwareMonitor
#pragma managed
#include "../Utils/LibreHardwareMonitorBridge.h"
#pragma unmanaged

bool TemperatureWrapper::initialized = false;
static GpuInfo* gpuInfo = nullptr;

void TemperatureWrapper::Initialize() {
    try {
        LibreHardwareMonitorBridge::Initialize();
        initialized = true;
        Logger::Debug("TemperatureWrapper: LibreHardwareMonitor initialized");
    } catch (...) {
        initialized = false;
        Logger::Error("TemperatureWrapper: LibreHardwareMonitor initialization failed");
    }
}

void TemperatureWrapper::Cleanup() {
    if (initialized) {
        LibreHardwareMonitorBridge::Cleanup();
        initialized = false;
    }
    if (gpuInfo) { delete gpuInfo; gpuInfo = nullptr; }
}

std::vector<std::pair<std::string, double>> TemperatureWrapper::GetTemperatures() {
    std::vector<std::pair<std::string, double>> temps;

    if (initialized) {
        try {
            auto libreTemps = LibreHardwareMonitorBridge::GetTemperatures();
            temps.insert(temps.end(), libreTemps.begin(), libreTemps.end());
        } catch (...) {
            // Ignore errors
        }
    }

    return temps;
}

bool TemperatureWrapper::IsInitialized() { return initialized; }

#elif defined(TCMT_MACOS)
// ======================== macOS Implementation ========================
// Temperature monitoring on macOS requires either:
// 1. Apple SMC access via IOKit (private API, requires specific entitlements)
// 2. Running `powermetrics` command-line tool
// 3. Third-party library like SMCKit
//
// For now, return empty temperatures. This will be implemented
// in a future update using IOKit SMC access or powermetrics parsing.

bool TemperatureWrapper::initialized = false;

void TemperatureWrapper::Initialize() {
    initialized = true;
    Logger::Info("TemperatureWrapper: macOS temperature monitoring enabled (limited support)");
}

void TemperatureWrapper::Cleanup() {
    initialized = false;
}

std::vector<std::pair<std::string, double>> TemperatureWrapper::GetTemperatures() {
    // On macOS, GPU temperature from GpuInfo's Metal-based detection.
    // Full SMC-based temperature monitoring is planned for future.
    std::vector<std::pair<std::string, double>> temps;
    return temps;
}

bool TemperatureWrapper::IsInitialized() { return initialized; }

#else
#error "Unsupported platform"
#endif
