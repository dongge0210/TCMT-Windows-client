#include "InfoFactory.h"
#include "../common/PlatformDefs.h"

// 平台特定包含 - 直接引用模块而不是platform子目录
#ifdef PLATFORM_WINDOWS
    #include "../windows/WinCpuInfo.h"
    #include "../windows/WinMemoryInfo.h"
    #include "../windows/WinGpuInfo.h"
    #include "../windows/WinNetworkAdapter.h"
    #include "../windows/WinDiskInfo.h"
    #include "../windows/WinTemperatureWrapper.h"
    #include "../windows/WinTpmInfo.h"
    #include "../windows/WinUsbInfo.h"
    #include "../windows/WinOSInfo.h"
    #include "../windows/WinDataCollector.h"
#elif defined(PLATFORM_MACOS)
    // macOS直接使用现有模块，包含平台检测
    #include "../cpu/CpuInfo.h"
    #include "../memory/MemoryInfo.h"
    #include "../gpu/GpuInfo.h"
    #include "../network/NetworkAdapter.h"
    #include "../disk/DiskInfo.h"
    #include "../temperature/TemperatureInfo.h"
    #include "../tpm/TpmInfo.h"
    #include "../usb/USBInfo.h"
    #include "../os/OSInfo.h"
#elif defined(PLATFORM_LINUX)
    #include "../linux/LinuxCpuInfo.h"
    #include "../linux/LinuxMemoryInfo.h"
    #include "../linux/LinuxGpuInfo.h"
    #include "../linux/LinuxNetworkAdapter.h"
    #include "../linux/LinuxDiskInfo.h"
    #include "../linux/LinuxTemperatureWrapper.h"
    #include "../linux/LinuxTpmInfo.h"
    #include "../linux/LinuxUsbInfo.h"
    #include "../linux/LinuxOSInfo.h"
    #include "../linux/LinuxDataCollector.h"
#endif

// 兼容性支持：包含原有类
#include "../cpu/CpuInfo.h"

#include <sstream>

// 静态成员初始化
std::string InfoFactory::lastError;
uint32_t InfoFactory::defaultUpdateInterval = 1000; // 默认1秒更新间隔

// 公共接口实现
std::unique_ptr<ICpuInfo> InfoFactory::CreateCpuInfo() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinCpuInfo();
        #elif defined(PLATFORM_MACOS)
            return CreateMacCpuInfo();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxCpuInfo();
        #else
            SetError("Unsupported platform for CPU info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create CPU info: ") + e.what());
        return nullptr;
    }
}

// 兼容性支持：创建原有类实例
std::unique_ptr<CpuInfo> InfoFactory::CreateLegacyCpuInfo() {
    try {
#ifdef PLATFORM_WINDOWS
        return std::make_unique<CpuInfo>();
#elif defined(PLATFORM_MACOS)
        // macOS不支持原有的CpuInfo类，返回nullptr
        SetError("Legacy CpuInfo not supported on macOS");
        return nullptr;
#else
        // 其他平台也不支持
        SetError("Legacy CpuInfo not supported on this platform");
        return nullptr;
#endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create legacy CPU info: ") + e.what());
        return nullptr;
    }
}

// 兼容性支持：创建CPU适配器
std::unique_ptr<ICpuAdapter> InfoFactory::CreateCpuAdapter() {
    try {
        #ifdef PLATFORM_WINDOWS
            return std::make_unique<WinCpuAdapter>();
        #elif defined(PLATFORM_MACOS)
            return CreateMacCpuAdapter();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxCpuAdapter();
        #else
            SetError("Unsupported platform for CPU adapter");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create CPU adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IMemoryInfo> InfoFactory::CreateMemoryInfo() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinMemoryInfo();
        #elif defined(PLATFORM_MACOS)
            return CreateMacMemoryInfo();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxMemoryInfo();
        #else
            SetError("Unsupported platform for memory info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create memory info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IGpuInfo> InfoFactory::CreateGpuInfo() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinGpuInfo();
        #elif defined(PLATFORM_MACOS)
            return CreateMacGpuInfo();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxGpuInfo();
        #else
            SetError("Unsupported platform for GPU info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create GPU info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<INetworkAdapter> InfoFactory::CreateNetworkAdapter(const std::string& adapterName) {
    try {
        std::string sanitizedName = SanitizeComponentName(adapterName);
        
        #ifdef PLATFORM_WINDOWS
            return CreateWinNetworkAdapter(sanitizedName);
        #elif defined(PLATFORM_MACOS)
            return CreateMacNetworkAdapter(sanitizedName);
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxNetworkAdapter(sanitizedName);
        #else
            SetError("Unsupported platform for network adapter info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create network adapter info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDiskInfo> InfoFactory::CreateDiskInfo(const std::string& diskName) {
    try {
        std::string sanitizedName = SanitizeComponentName(diskName);
        
        #ifdef PLATFORM_WINDOWS
            return CreateWinDiskInfo(sanitizedName);
        #elif defined(PLATFORM_MACOS)
            return CreateMacDiskInfo(sanitizedName);
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxDiskInfo(sanitizedName);
        #else
            SetError("Unsupported platform for disk info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create disk info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITemperatureMonitor> InfoFactory::CreateTemperatureMonitor() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinTemperatureMonitor();
        #elif defined(PLATFORM_MACOS)
            return CreateMacTemperatureMonitor();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxTemperatureMonitor();
        #else
            SetError("Unsupported platform for temperature monitor");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create temperature monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITpmInfo> InfoFactory::CreateTpmInfo() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinTpmInfo();
        #elif defined(PLATFORM_MACOS)
            return CreateMacTpmInfo();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxTpmInfo();
        #else
            SetError("Unsupported platform for TPM info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create TPM info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IUsbMonitor> InfoFactory::CreateUsbMonitor() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinUsbMonitor();
        #elif defined(PLATFORM_MACOS)
            return CreateMacUsbMonitor();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxUsbMonitor();
        #else
            SetError("Unsupported platform for USB monitor");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create USB monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IOSInfo> InfoFactory::CreateOSInfo() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinOSInfo();
        #elif defined(PLATFORM_MACOS)
            return CreateMacOSInfo();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxOSInfo();
        #else
            SetError("Unsupported platform for OS info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create OS info: ") + e.what());
        return nullptr;
    }
}



std::unique_ptr<ISystemInfo> InfoFactory::CreateSystemInfo() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinSystemInfo();
        #elif defined(PLATFORM_MACOS)
            auto systemInfo = std::make_unique<MacSystemInfo>();
            if (systemInfo && systemInfo->Initialize()) {
                return systemInfo;
            } else {
                SetError("Failed to initialize macOS system info");
                return nullptr;
            }
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxSystemInfo();
        #else
            SetError("Unsupported platform for system info");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create system info: ") + e.what());
        return nullptr;
    }
}



std::unique_ptr<IDataCollector> InfoFactory::CreateDataCollector() {
    try {
        #ifdef PLATFORM_WINDOWS
            return CreateWinDataCollector();
        #elif defined(PLATFORM_MACOS)
            return CreateMacDataCollector();
        #elif defined(PLATFORM_LINUX)
            return CreateLinuxDataCollector();
        #else
            SetError("Unsupported platform for data collector");
            return nullptr;
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create data collector: ") + e.what());
        return nullptr;
    }
}

std::vector<std::string> InfoFactory::GetAvailableNetworkAdapters() {
    std::vector<std::string> adapters;
    
    try {
        #ifdef PLATFORM_WINDOWS
            // Windows特定实现
            // 这里需要调用Windows API获取网络适配器列表
            // 暂时返回空列表，具体实现需要移到WinNetworkAdapter中
        #elif defined(PLATFORM_MACOS)
            // macOS特定实现
            // 使用getifaddrs()等API获取网络接口列表
        #elif defined(PLATFORM_LINUX)
            // Linux特定实现
            // 读取/proc/net/dev或使用netlink获取网络接口列表
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to get network adapters: ") + e.what());
    }
    
    return adapters;
}

std::vector<std::string> InfoFactory::GetAvailableDisks() {
    std::vector<std::string> disks;
    
    try {
        #ifdef PLATFORM_WINDOWS
            // Windows特定实现
            // 使用GetLogicalDriveStrings()等API获取磁盘列表
        #elif defined(PLATFORM_MACOS)
            // macOS特定实现
            // 使用IOKit获取磁盘设备列表
        #elif defined(PLATFORM_LINUX)
            // Linux特定实现
            // 读取/proc/mounts或使用udev获取磁盘列表
        #endif
    } catch (const std::exception& e) {
        SetError(std::string("Failed to get disks: ") + e.what());
    }
    
    return disks;
}

bool InfoFactory::IsGpuMonitoringAvailable() {
    #ifdef PLATFORM_WINDOWS
        // Windows通常有GPU监控支持
        return true;
    #elif defined(PLATFORM_MACOS)
        // macOS有Metal API支持
        return true;
    #elif defined(PLATFORM_LINUX)
        // Linux需要检查DRM/NVML/ADL支持
        // 这里可以检查相关文件是否存在
        return true; // 暂时返回true，具体实现需要检查
    #else
        return false;
    #endif
}

bool InfoFactory::IsTpmAvailable() {
    #ifdef PLATFORM_WINDOWS
        // Windows有TPM支持
        return true;
    #elif defined(PLATFORM_MACOS)
        // macOS通常不使用TPM
        return false;
    #elif defined(PLATFORM_LINUX)
        // Linux需要检查TPM设备
        return true; // 暂时返回true，需要检查/dev/tpm*等设备
    #else
        return false;
    #endif
}

bool InfoFactory::IsUsbMonitoringAvailable() {
    #ifdef PLATFORM_WINDOWS
        // Windows有USB监控支持
        return true;
    #elif defined(PLATFORM_MACOS)
        // macOS有IOKit USB监控支持
        return true;
    #elif defined(PLATFORM_LINUX)
        // Linux有udev USB监控支持
        return true;
    #else
        return false;
    #endif
}

bool InfoFactory::IsTemperatureMonitoringAvailable() {
    #ifdef PLATFORM_WINDOWS
        // Windows有温度监控支持
        return true;
    #elif defined(PLATFORM_MACOS)
        // macOS有IOKit温度传感器支持
        return true;
    #elif defined(PLATFORM_LINUX)
        // Linux有hwmon温度监控支持
        return true;
    #else
        return false;
    #endif
}

std::string InfoFactory::GetPlatformName() {
    return PLATFORM_NAME;
}

std::string InfoFactory::GetCompilerName() {
    return COMPILER_NAME;
}

std::string InfoFactory::GetArchitectureName() {
    return ARCH_NAME;
}

std::string InfoFactory::GetBuildInfo() {
    std::ostringstream oss;
    oss << "Platform: " << GetPlatformName() 
        << ", Compiler: " << GetCompilerName()
        << ", Architecture: " << GetArchitectureName()
        << ", Build: " << BUILD_TIMESTAMP;
    return oss.str();
}

void InfoFactory::SetDefaultUpdateInterval(uint32_t intervalMs) {
    defaultUpdateInterval = intervalMs;
}

uint32_t InfoFactory::GetDefaultUpdateInterval() {
    return defaultUpdateInterval;
}

std::string InfoFactory::GetLastError() {
    return lastError;
}

void InfoFactory::ClearError() {
    lastError.clear();
}

// 平台特定的创建函数实现
#ifdef PLATFORM_WINDOWS
std::unique_ptr<ICpuInfo> InfoFactory::CreateWinCpuInfo() {
    try {
        auto cpuInfo = std::make_unique<WinCpuInfo>();
        if (cpuInfo && cpuInfo->Initialize()) {
            return cpuInfo;
        } else {
            SetError("Failed to initialize Windows CPU info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows CPU info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ICpuAdapter> InfoFactory::CreateWinCpuAdapter() {
    try {
        auto adapter = std::make_unique<WinCpuAdapter>();
        if (adapter && adapter->Initialize()) {
            return adapter;
        } else {
            SetError("Failed to initialize Windows CPU adapter");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows CPU adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IMemoryInfo> InfoFactory::CreateWinMemoryInfo() {
    try {
        auto memoryInfo = std::make_unique<WinMemoryInfo>();
        if (memoryInfo && memoryInfo->Initialize()) {
            return memoryInfo;
        } else {
            SetError("Failed to initialize Windows memory info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows memory info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IGpuInfo> InfoFactory::CreateWinGpuInfo() {
    try {
        auto gpuInfo = std::make_unique<WinGpuInfo>();
        if (gpuInfo && gpuInfo->Initialize()) {
            return gpuInfo;
        } else {
            SetError("Failed to initialize Windows GPU info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows GPU info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<INetworkAdapter> InfoFactory::CreateWinNetworkAdapter(const std::string& adapterName) {
    try {
        auto adapter = std::make_unique<WinNetworkAdapter>(adapterName);
        if (adapter && adapter->Initialize()) {
            return adapter;
        } else {
            SetError("Failed to initialize Windows network adapter");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows network adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDiskInfo> InfoFactory::CreateWinDiskInfo(const std::string& diskName) {
    try {
        auto diskInfo = std::make_unique<WinDiskInfo>(diskName);
        if (diskInfo && diskInfo->Initialize()) {
            return diskInfo;
        } else {
            SetError("Failed to initialize Windows disk info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows disk info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITemperatureMonitor> InfoFactory::CreateWinTemperatureMonitor() {
    try {
        auto monitor = std::make_unique<WinTemperatureWrapper>();
        if (monitor && monitor->Initialize()) {
            return monitor;
        } else {
            SetError("Failed to initialize Windows temperature monitor");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows temperature monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITpmInfo> InfoFactory::CreateWinTpmInfo() {
    try {
        auto tpmInfo = std::make_unique<WinTpmInfo>();
        if (tpmInfo && tpmInfo->Initialize()) {
            return tpmInfo;
        } else {
            SetError("Failed to initialize Windows TPM info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows TPM info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IUsbMonitor> InfoFactory::CreateWinUsbMonitor() {
    try {
        auto usbMonitor = std::make_unique<WinUsbInfo>();
        if (usbMonitor && usbMonitor->Initialize()) {
            return usbMonitor;
        } else {
            SetError("Failed to initialize Windows USB monitor");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows USB monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IOSInfo> InfoFactory::CreateWinOSInfo() {
    try {
        auto osInfo = std::make_unique<WinOSInfo>();
        if (osInfo && osInfo->Initialize()) {
            return osInfo;
        } else {
            SetError("Failed to initialize Windows OS info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows OS info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ISystemInfo> InfoFactory::CreateWinSystemInfo() {
    try {
        auto systemInfo = std::make_unique<WinSystemInfo>();
        if (systemInfo && systemInfo->Initialize()) {
            return systemInfo;
        } else {
            SetError("Failed to initialize Windows system info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows system info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IBatteryInfo> InfoFactory::CreateWinBatteryInfo() {
    try {
        auto batteryInfo = std::make_unique<WinBatteryInfo>();
        if (batteryInfo && batteryInfo->Initialize()) {
            return batteryInfo;
        } else {
            SetError("Failed to initialize Windows battery info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows battery info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITemperatureInfo> InfoFactory::CreateWinTemperatureInfo() {
    try {
        auto tempInfo = std::make_unique<WinTemperatureInfo>();
        if (tempInfo && tempInfo->Initialize()) {
            return tempInfo;
        } else {
            SetError("Failed to initialize Windows temperature info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows temperature info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDiskInfo> InfoFactory::CreateWinDiskInfo() {
    try {
        auto diskInfo = std::make_unique<WinDiskInfo>();
        if (diskInfo && diskInfo->Initialize()) {
            return diskInfo;
        } else {
            SetError("Failed to initialize Windows disk info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows disk info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<INetworkInfo> InfoFactory::CreateWinNetworkInfo() {
    try {
        auto networkInfo = std::make_unique<WinNetworkInfo>();
        if (networkInfo && networkInfo->Initialize()) {
            return networkInfo;
        } else {
            SetError("Failed to initialize Windows network info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows network info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDataCollector> InfoFactory::CreateWinDataCollector() {
    try {
        auto dataCollector = std::make_unique<WinDataCollector>();
        if (dataCollector && dataCollector->Initialize()) {
            return dataCollector;
        } else {
            SetError("Failed to initialize Windows data collector");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Windows data collector: ") + e.what());
        return nullptr;
    }
}
#elif defined(PLATFORM_MACOS)
std::unique_ptr<ICpuInfo> InfoFactory::CreateMacCpuInfo() {
    try {
        auto cpuInfo = std::make_unique<MacCpuInfo>();
        if (cpuInfo && cpuInfo->Initialize()) {
            return cpuInfo;
        } else {
            SetError("Failed to initialize macOS CPU info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS CPU info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ICpuAdapter> InfoFactory::CreateMacCpuAdapter() {
    try {
        auto adapter = std::make_unique<MacCpuAdapter>();
        if (adapter && adapter->Initialize()) {
            return adapter;
        } else {
            SetError("Failed to initialize macOS CPU adapter");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS CPU adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IMemoryInfo> InfoFactory::CreateMacMemoryInfo() {
    auto memoryInfo = std::make_unique<MacMemoryInfo>();
    if (memoryInfo && memoryInfo->Initialize()) {
        return memoryInfo;
    } else {
        SetError("Failed to initialize macOS memory info");
        return nullptr;
    }
}

// 兼容性支持：创建MacMemoryAdapter
std::unique_ptr<MacMemoryAdapter> InfoFactory::CreateMacMemoryAdapter() {
    auto adapter = std::make_unique<MacMemoryAdapter>();
    if (adapter && adapter->Initialize()) {
        return adapter;
    } else {
        SetError("Failed to initialize macOS memory adapter");
        return nullptr;
    }
}

std::unique_ptr<IGpuInfo> InfoFactory::CreateMacGpuInfo() {
    auto gpuInfo = std::make_unique<MacGpuInfo>();
    if (gpuInfo && gpuInfo->Initialize()) {
        return gpuInfo;
    } else {
        SetError("Failed to initialize macOS GPU info");
        return nullptr;
    }
}

// 兼容性支持：创建MacGpuAdapter
std::unique_ptr<MacGpuAdapter> InfoFactory::CreateMacGpuAdapter() {
    auto adapter = std::make_unique<MacGpuAdapter>();
    if (adapter && adapter->Initialize()) {
        return adapter;
    } else {
        SetError("Failed to initialize macOS GPU adapter");
        return nullptr;
    }
}

std::unique_ptr<INetworkAdapter> InfoFactory::CreateMacNetworkAdapter(const std::string& adapterName) {
    try {
        // TODO: Implement MacNetworkAdapter class
        SetError("MacNetworkAdapter not implemented yet");
        return nullptr;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS network adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDiskInfo> InfoFactory::CreateMacDiskInfo(const std::string& diskName) {
    try {
        auto diskInfo = std::make_unique<MacDiskInfo>(diskName);
        if (diskInfo && diskInfo->Initialize()) {
            return diskInfo;
        } else {
            SetError("Failed to initialize macOS disk info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS disk info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITemperatureMonitor> InfoFactory::CreateMacTemperatureMonitor() {
    try {
        // TODO: Implement MacTemperatureMonitor class
        SetError("MacTemperatureMonitor not implemented yet");
        return nullptr;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS temperature monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITpmInfo> InfoFactory::CreateMacTpmInfo() {
    try {
        // TODO: Implement MacTpmInfo class
        SetError("MacTpmInfo not implemented yet");
        return nullptr;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS TPM info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IUsbMonitor> InfoFactory::CreateMacUsbMonitor() {
    try {
        // TODO: Implement MacUsbMonitor class
        SetError("MacUsbMonitor not implemented yet");
        return nullptr;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS USB monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IOSInfo> InfoFactory::CreateMacOSInfo() {
    try {
        // TODO: Implement MacOSInfo class
        SetError("MacOSInfo not implemented yet");
        return nullptr;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS OS info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ISystemInfo> InfoFactory::CreateMacSystemInfo() {
    try {
        auto systemInfo = std::make_unique<MacSystemInfo>();
        if (systemInfo && systemInfo->Initialize()) {
            return systemInfo;
        } else {
            SetError("Failed to initialize macOS system info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS system info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IBatteryInfo> InfoFactory::CreateMacBatteryInfo() {
    try {
        auto batteryInfo = std::make_unique<MacBatteryInfo>();
        if (batteryInfo && batteryInfo->Initialize()) {
            return batteryInfo;
        } else {
            SetError("Failed to initialize macOS battery info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS battery info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDataCollector> InfoFactory::CreateMacDataCollector() {
    try {
        // TODO: Implement MacDataCollector class
        SetError("MacDataCollector not implemented yet");
        return nullptr;
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create macOS data collector: ") + e.what());
        return nullptr;
    }
}
#elif defined(PLATFORM_LINUX)
std::unique_ptr<ICpuInfo> InfoFactory::CreateLinuxCpuInfo() {
    try {
        auto cpuInfo = std::make_unique<LinuxCpuInfo>();
        if (cpuInfo && cpuInfo->Initialize()) {
            return cpuInfo;
        } else {
            SetError("Failed to initialize Linux CPU info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux CPU info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ICpuAdapter> InfoFactory::CreateLinuxCpuAdapter() {
    try {
        auto adapter = std::make_unique<LinuxCpuAdapter>();
        if (adapter && adapter->Initialize()) {
            return adapter;
        } else {
            SetError("Failed to initialize Linux CPU adapter");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux CPU adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IMemoryInfo> InfoFactory::CreateLinuxMemoryInfo() {
    try {
        auto memoryInfo = std::make_unique<LinuxMemoryInfo>();
        if (memoryInfo && memoryInfo->Initialize()) {
            return memoryInfo;
        } else {
            SetError("Failed to initialize Linux memory info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux memory info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IGpuInfo> InfoFactory::CreateLinuxGpuInfo() {
    try {
        auto gpuInfo = std::make_unique<LinuxGpuInfo>();
        if (gpuInfo && gpuInfo->Initialize()) {
            return gpuInfo;
        } else {
            SetError("Failed to initialize Linux GPU info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux GPU info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<INetworkAdapter> InfoFactory::CreateLinuxNetworkAdapter(const std::string& adapterName) {
    try {
        auto adapter = std::make_unique<LinuxNetworkAdapter>(adapterName);
        if (adapter && adapter->Initialize()) {
            return adapter;
        } else {
            SetError("Failed to initialize Linux network adapter");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux network adapter: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDiskInfo> InfoFactory::CreateLinuxDiskInfo(const std::string& diskName) {
    try {
        auto diskInfo = std::make_unique<LinuxDiskInfo>(diskName);
        if (diskInfo && diskInfo->Initialize()) {
            return diskInfo;
        } else {
            SetError("Failed to initialize Linux disk info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux disk info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITemperatureMonitor> InfoFactory::CreateLinuxTemperatureMonitor() {
    try {
        auto monitor = std::make_unique<LinuxTemperatureWrapper>();
        if (monitor && monitor->Initialize()) {
            return monitor;
        } else {
            SetError("Failed to initialize Linux temperature monitor");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux temperature monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITpmInfo> InfoFactory::CreateLinuxTpmInfo() {
    try {
        auto tpmInfo = std::make_unique<LinuxTpmInfo>();
        if (tpmInfo && tpmInfo->Initialize()) {
            return tpmInfo;
        } else {
            SetError("Failed to initialize Linux TPM info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux TPM info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IUsbMonitor> InfoFactory::CreateLinuxUsbMonitor() {
    try {
        auto usbMonitor = std::make_unique<LinuxUsbInfo>();
        if (usbMonitor && usbMonitor->Initialize()) {
            return usbMonitor;
        } else {
            SetError("Failed to initialize Linux USB monitor");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux USB monitor: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IOSInfo> InfoFactory::CreateLinuxOSInfo() {
    try {
        auto osInfo = std::make_unique<LinuxOSInfo>();
        if (osInfo && osInfo->Initialize()) {
            return osInfo;
        } else {
            SetError("Failed to initialize Linux OS info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux OS info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ISystemInfo> InfoFactory::CreateLinuxSystemInfo() {
    try {
        auto systemInfo = std::make_unique<LinuxSystemInfo>();
        if (systemInfo && systemInfo->Initialize()) {
            return systemInfo;
        } else {
            SetError("Failed to initialize Linux system info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux system info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IBatteryInfo> InfoFactory::CreateLinuxBatteryInfo() {
    try {
        auto batteryInfo = std::make_unique<LinuxBatteryInfo>();
        if (batteryInfo && batteryInfo->Initialize()) {
            return batteryInfo;
        } else {
            SetError("Failed to initialize Linux battery info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux battery info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<ITemperatureInfo> InfoFactory::CreateLinuxTemperatureInfo() {
    try {
        auto tempInfo = std::make_unique<LinuxTemperatureInfo>();
        if (tempInfo && tempInfo->Initialize()) {
            return tempInfo;
        } else {
            SetError("Failed to initialize Linux temperature info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux temperature info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDiskInfo> InfoFactory::CreateLinuxDiskInfo() {
    try {
        auto diskInfo = std::make_unique<LinuxDiskInfo>();
        if (diskInfo && diskInfo->Initialize()) {
            return diskInfo;
        } else {
            SetError("Failed to initialize Linux disk info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux disk info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<INetworkInfo> InfoFactory::CreateLinuxNetworkInfo() {
    try {
        auto networkInfo = std::make_unique<LinuxNetworkInfo>();
        if (networkInfo && networkInfo->Initialize()) {
            return networkInfo;
        } else {
            SetError("Failed to initialize Linux network info");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux network info: ") + e.what());
        return nullptr;
    }
}

std::unique_ptr<IDataCollector> InfoFactory::CreateLinuxDataCollector() {
    try {
        auto dataCollector = std::make_unique<LinuxDataCollector>();
        if (dataCollector && dataCollector->Initialize()) {
            return dataCollector;
        } else {
            SetError("Failed to initialize Linux data collector");
            return nullptr;
        }
    } catch (const std::exception& e) {
        SetError(std::string("Failed to create Linux data collector: ") + e.what());
        return nullptr;
    }
}
#endif

// 私有辅助函数实现
void InfoFactory::SetError(const std::string& error) {
    lastError = error;
}

bool InfoFactory::ValidateComponentName(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    
    // 检查名称是否包含非法字符
    const std::string invalidChars = "<>:\"|?*";
    return name.find_first_of(invalidChars) == std::string::npos;
}

std::string InfoFactory::SanitizeComponentName(const std::string& name) {
    if (name.empty()) {
        return name;
    }
    
    std::string sanitized = name;
    
    #ifdef PLATFORM_WINDOWS
        // Windows特定清理
        // 移除驱动器前缀（如果存在）
        if (sanitized.length() >= 2 && sanitized[1] == ':') {
            sanitized = sanitized.substr(2);
        }
        
        // 移除开头的反斜杠
        while (!sanitized.empty() && (sanitized[0] == '\\' || sanitized[0] == '/')) {
            sanitized = sanitized.substr(1);
        }
    #elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        // Unix-like系统特定清理
        // 移除开头的斜杠
        while (!sanitized.empty() && sanitized[0] == '/') {
            sanitized = sanitized.substr(1);
        }
    #endif
    
    return sanitized;
}