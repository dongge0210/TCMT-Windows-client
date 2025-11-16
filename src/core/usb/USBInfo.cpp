#include "USBInfo.h"
#include "../Utils/Logger.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <filesystem>
    #include <set>
    #include <bitset>
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    
    // Missing DRIVE_USB constant (not defined in Windows headers)
    #ifndef DRIVE_USB
    #define DRIVE_USB 4
    #endif
#elif defined(PLATFORM_MACOS)
    #include <IOKit/IOKitLib.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <diskarbitration/diskarbitration.h>
    #include <sys/mount.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <thread>
    #include <atomic>
    #include <mutex>
    #include <chrono>
    #include <set>
    #include <fstream>
    #include <sstream>
    #include <dirent.h>
    using namespace std::chrono_literals;
#elif defined(PLATFORM_LINUX)
    #include <libudev.h>
    #include <sys/mount.h>
    #include <unistd.h>
    #include <thread>
    #include <atomic>
    #include <mutex>
    #include <chrono>
    #include <set>
    #include <fstream>
    #include <sstream>
    #include <dirent.h>
    using namespace std::chrono_literals;
#endif

// USB monitoring implementation class
class USBInfoManager::USBMonitorImpl {
public:
    USBMonitorImpl(USBInfoManager* parent) : parentManager(parent), stopFlag(false) {}
    
    ~USBMonitorImpl() {
        StopMonitoring();
    }
    
    void StartMonitoring() {
        if (monitorThread.joinable()) return;
        
        stopFlag = false;
        monitorThread = std::thread(&USBMonitorImpl::MonitorLoop, this);
        Logger::Info("USB monitoring thread started");
    }
    
    void StopMonitoring() {
        stopFlag = true;
        if (monitorThread.joinable()) {
            monitorThread.join();
            Logger::Info("USB monitoring thread stopped");
        }
    }
    
    std::vector<USBDeviceInfo> GetCurrentDevices() {
        std::lock_guard<std::mutex> lock(devicesMutex);
        return currentDevices;
    }
    
private:
    void MonitorLoop() {
        while (!stopFlag) {
            try {
#ifdef PLATFORM_WINDOWS
                DetectUSBChanges();
                std::this_thread::sleep_for(500ms);
#elif defined(PLATFORM_MACOS)
                MonitorMacUSBDevices();
                std::this_thread::sleep_for(500ms);
#elif defined(PLATFORM_LINUX)
                MonitorLinuxUSBDevices();
                std::this_thread::sleep_for(500ms);
#endif
            }
            catch (const std::exception& e) {
                Logger::Error("USB monitoring loop exception: " + std::string(e.what()));
                std::this_thread::sleep_for(1s);
            }
        }
    }
    
    // macOS USB监控实现
#ifdef PLATFORM_MACOS
    void MonitorMacUSBDevices() {
        try {
            // 获取当前挂载的USB设备
            struct statfs *mounts;
            int count = getmntinfo(&mounts, MNT_WAIT);
            if (count == 0) return;
            
            std::set<std::string> newMountPoints;
            
            for (int i = 0; i < count; i++) {
                std::string mountPoint = mounts[i].f_mntonname;
                std::string devicePath = mounts[i].f_mntfromname;
                
                // 检查是否为USB设备
                if (devicePath.find("/dev/disk") == 0) {
                    std::string actualDevicePath = parentManager->GetMacDevicePath(mountPoint);
                    if (!actualDevicePath.empty() && parentManager->IsMacUSBDevice(actualDevicePath)) {
                        newMountPoints.insert(mountPoint);
                    }
                }
            }
            
            // 比较新旧设备列表
            {
                std::lock_guard<std::mutex> lock(devicesMutex);
                std::set<std::string> oldMountPoints;
                for (const auto& device : currentDevices) {
                    oldMountPoints.insert(device.mountPoint);
                }
                
                // 检测新插入的设备
                for (const auto& mountPoint : newMountPoints) {
                    if (oldMountPoints.find(mountPoint) == oldMountPoints.end()) {
                        USBDeviceInfo info;
                        if (parentManager->GetMacDeviceInfo(mountPoint, info)) {
                            info.state = USBState::Inserted;
                            info.lastUpdate = time(nullptr);
                            currentDevices.push_back(info);
                            Logger::Info("USB inserted: " + mountPoint + " (" + info.volumeLabel + ")");
                            
                            if (parentManager->stateCallback) {
                                parentManager->stateCallback(info);
                            }
                        }
                    }
                }
                
                // 检测拔出的设备
                for (const auto& mountPoint : oldMountPoints) {
                    if (newMountPoints.find(mountPoint) == newMountPoints.end()) {
                        for (auto it = currentDevices.begin(); it != currentDevices.end(); ++it) {
                            if (it->mountPoint == mountPoint) {
                                it->state = USBState::Removed;
                                it->lastUpdate = time(nullptr);
                                Logger::Info("USB removed: " + mountPoint + " (" + it->volumeLabel + ")");
                                
                                if (parentManager->stateCallback) {
                                    parentManager->stateCallback(*it);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        catch (const std::exception& e) {
            Logger::Error("MonitorMacUSBDevices failed: " + std::string(e.what()));
        }
    }
#endif
    
    // Windows USB监控实现
#ifdef PLATFORM_WINDOWS
    void DetectUSBChanges();
#endif
    
    USBInfoManager* parentManager;
    std::thread monitorThread;
    std::atomic<bool> stopFlag;
    std::set<std::string> currentUSBPaths;
    std::vector<USBDeviceInfo> currentDevices;
    std::mutex devicesMutex;
};



#ifdef PLATFORM_MACOS
std::string USBInfoManager::GetMacDevicePath(const std::string& mountPoint) {
    try {
        // 通过diskutil获取设备路径
        std::string command = "diskutil info " + mountPoint + " | grep \"Device Node\"";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";
        
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            size_t pos = line.find("Device Node:");
            if (pos != std::string::npos) {
                std::string devicePath = line.substr(pos + 12);
                // 移除前后空格
                devicePath.erase(0, devicePath.find_first_not_of(" \t\n\r"));
                devicePath.erase(devicePath.find_last_not_of(" \t\n\r") + 1);
                pclose(pipe);
                return devicePath;

            }
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacDevicePath failed: " + std::string(e.what()));
    }
    
    return "";
}
#endif

#ifdef PLATFORM_MACOS
bool USBInfoManager::IsMacUSBDevice(const std::string& devicePath) {
    try {
        // 首先过滤掉系统卷
        if (devicePath.find("/dev/disk1") == 0) {
            // disk1 通常是系统卷
            return false;
        }
        
        // 检查设备是否为外部设备
        std::string command = "diskutil info " + devicePath + " 2>/dev/null | grep -E \"External|Removable|Protocol\"";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return false;
        
        char buffer[256];
        bool isExternal = false;
        bool isUSB = false;
        
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            if (line.find("External") != std::string::npos) {
                isExternal = true;
            }
            if (line.find("USB") != std::string::npos || line.find("Removable Media") != std::string::npos) {
                isUSB = true;
            }
        }
        pclose(pipe);
        
        // 只有同时是外部设备和USB/可移动媒体才认为是USB设备
        return isExternal && isUSB;
    }
    catch (const std::exception& e) {
        Logger::Error("IsMacUSBDevice failed: " + std::string(e.what()));
    }
    
    return false;
}
#endif

#ifdef PLATFORM_MACOS
void USBInfoManager::GetMacDeviceProperties(const std::string& devicePath, USBDeviceInfo& info) {
    try {
        // 获取设备详细信息
        std::string command = "diskutil info " + devicePath;
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return;
        
        char buffer[512];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        
        // 解析设备信息
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Device Node:") != std::string::npos) {
                info.devicePath = line.substr(line.find(":") + 2);
                info.devicePath.erase(0, info.devicePath.find_first_not_of(" \t"));
                info.devicePath.erase(info.devicePath.find_last_not_of(" \t\n\r") + 1);
            }
            else if (line.find("Volume Name:") != std::string::npos) {
                info.volumeLabel = line.substr(line.find(":") + 2);
                info.volumeLabel.erase(0, info.volumeLabel.find_first_not_of(" \t"));
                info.volumeLabel.erase(info.volumeLabel.find_last_not_of(" \t\n\r") + 1);
            }
            else if (line.find("Total Size:") != std::string::npos) {
                std::string sizeStr = line.substr(line.find(":") + 2);
                // 解析大小（如 "500.1 GB (500102802048 bytes)"）
                size_t spacePos = sizeStr.find(" ");
                if (spacePos != std::string::npos) {
                    std::string sizeValue = sizeStr.substr(0, spacePos);
                    info.totalSize = static_cast<uint64_t>(std::stod(sizeValue) * (1024LL * 1024 * 1024));
                }
            }
            else if (line.find("Free Space:") != std::string::npos) {
                std::string sizeStr = line.substr(line.find(":") + 2);
                size_t spacePos = sizeStr.find(" ");
                if (spacePos != std::string::npos) {
                    std::string sizeValue = sizeStr.substr(0, spacePos);
                    info.freeSpace = static_cast<uint64_t>(std::stod(sizeValue) * (1024LL * 1024 * 1024));
                }
            }
        }
        
        info.usedSpace = info.totalSize - info.freeSpace;
        info.isRemovable = true;
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacDeviceProperties failed: " + std::string(e.what()));
    }
#else
    // Windows和其他平台的空实现
    (void)devicePath; // 避免未使用参数警告
    (void)info;
#endif
}

#ifdef PLATFORM_MACOS
bool USBInfoManager::GetMacDeviceInfo(const std::string& mountPoint, USBDeviceInfo& info) {
    try {
        info.mountPoint = mountPoint;
        info.drivePath = mountPoint; // macOS使用挂载点作为驱动路径
        
        // 获取设备路径
        info.devicePath = GetMacDevicePath(mountPoint);
        if (info.devicePath.empty()) {
            Logger::Warn("Cannot get device path for mount point: " + mountPoint);
            return false;
        }
        
        // 检查是否为USB设备
        if (!IsMacUSBDevice(info.devicePath)) {
            return false;
        }
        
        // 获取设备属性
        GetMacDeviceProperties(info.devicePath, info);
        
        // 检查update文件夹
        info.isUpdateReady = HasUpdateFolder(mountPoint);
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacDeviceInfo failed: " + std::string(e.what()));
        return false;
    }
}
#endif

#ifdef PLATFORM_MACOS
DASessionRef USBInfoManager::CreateDiskArbitrationSession() {
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    return session;
}

void USBInfoManager::ReleaseDiskArbitrationSession(DASessionRef session) {
    if (session) {
        CFRelease(session);
    }
}
#else
// Windows和其他平台的空实现

#endif

// Windows实现
#ifdef PLATFORM_WINDOWS
void USBInfoManager::USBMonitorImpl::DetectUSBChanges() {
    std::set<std::string> newUSBPaths;
    DWORD drives = GetLogicalDrives();
    std::bitset<26> currentDrives(drives);
    
    // 检测所有可移动驱动器
    for (int i = 0; i < 26; ++i) {
        if (currentDrives[i]) {
            char driveLetter = 'A' + i;
            if (IsRemovableDrive(driveLetter)) {
                std::string drivePath = std::string(1, driveLetter) + ":\\";
                newUSBPaths.insert(drivePath);
            }
        }
    }
    
    // 比较新旧设备列表
    {
        std::lock_guard<std::mutex> lock(devicesMutex);

            
            // 检查新插入的设备
            for (const auto& path : newUSBPaths) {
                if (currentUSBPaths.find(path) == currentUSBPaths.end()) {
                    // New device inserted
                    USBDeviceInfo deviceInfo;
                    if (parentManager->GetDriveInfo(path, deviceInfo)) {
                        deviceInfo.state = USBState::Inserted;
                        if (deviceInfo.isUpdateReady) {
                            deviceInfo.state = USBState::UpdateReady;
                            Logger::Info("Update USB drive detected: " + path);
                        } else {
                            Logger::Info("USB device inserted: " + path);
                        }
                        
                        currentDevices.push_back(deviceInfo);
                        currentUSBPaths.insert(path);
                        
                        // Notify state change
                        if (parentManager->stateCallback) {
                            parentManager->stateCallback(deviceInfo);
                        }
                    }
                }
            }
            
            // Check for removed devices
            for (auto it = currentDevices.begin(); it != currentDevices.end();) {
                if (newUSBPaths.find(it->drivePath) == newUSBPaths.end()) {
                    // Device removed
                    Logger::Info("USB device removed: " + it->drivePath);
                    
                    USBDeviceInfo removedDevice = *it;
                    removedDevice.state = USBState::Removed;
                    
                    if (parentManager->stateCallback) {
                        parentManager->stateCallback(removedDevice);
                    }
                    
                    currentUSBPaths.erase(it->drivePath);
                    it = currentDevices.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    
    bool IsRemovableDrive(char letter) {
        char rootPath[] = "A:\\";
        rootPath[0] = letter;
        UINT driveType = GetDriveTypeA(rootPath);
        return driveType == DRIVE_REMOVABLE;
    }
    
    USBInfoManager* parentManager;
    std::thread monitorThread;
    std::atomic<bool> stopFlag;
    std::set<std::string> currentUSBPaths;
    std::vector<USBDeviceInfo> currentDevices;
    std::mutex devicesMutex;
};
#endif

// USBInfoManager 实现
USBInfoManager::USBInfoManager() : pImpl(std::make_unique<USBMonitorImpl>(this)) {}

USBInfoManager::~USBInfoManager() {
    Cleanup();
}

bool USBInfoManager::Initialize() {
    try {
        Logger::Info("Initializing USB monitoring manager");
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("USB monitoring manager initialization failed: " + std::string(e.what()));
        return false;
    }
}



void USBInfoManager::Cleanup() {
    StopMonitoring();
    Logger::Info("USB monitoring manager cleaned up");
}

void USBInfoManager::StartMonitoring() {
    if (pImpl) {
        pImpl->StartMonitoring();
    }
}

void USBInfoManager::StopMonitoring() {
    if (pImpl) {
        pImpl->StopMonitoring();
    }
}

std::vector<USBDeviceInfo> USBInfoManager::GetCurrentUSBDevices() {
    if (pImpl) {
        return pImpl->GetCurrentDevices();
    }
    return {};
}

void USBInfoManager::SetStateCallback(USBStateCallback callback) {
    stateCallback = callback;
}

bool USBInfoManager::IsInitialized() const {
    return pImpl != nullptr;
}

bool USBInfoManager::GetDeviceInfo(const std::string& identifier, USBDeviceInfo& info) {
    try {
#ifdef PLATFORM_WINDOWS
        return GetDriveInfo(identifier, info);
#elif defined(PLATFORM_MACOS)
        return GetMacDeviceInfo(identifier, info);
#elif defined(PLATFORM_LINUX)
        return GetLinuxDeviceInfo(identifier, info);
#else
        return false;
#endif
    }
    catch (const std::exception& e) {
        Logger::Error("Failed to get device information: " + std::string(e.what()));
        return false;
    }
}

#ifdef PLATFORM_WINDOWS
bool USBInfoManager::GetDriveInfo(const std::string& drivePath, USBDeviceInfo& info) {
    try {
        info.drivePath = drivePath;
        
        // 获取卷标
        char volumeName[MAX_PATH] = {0};
        if (GetVolumeInformationA(drivePath.c_str(), volumeName, MAX_PATH, 
                                   nullptr, nullptr, nullptr, nullptr, 0)) {
            info.volumeLabel = volumeName;
        }
        
        // 获取磁盘空间信息
        ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceExA(drivePath.c_str(), &freeBytesAvailable, 
                                 &totalBytes, &totalFreeBytes)) {
            info.totalSize = totalBytes.QuadPart;
            info.freeSpace = freeBytesAvailable.QuadPart;
        }
        
        // 检查update文件夹
        info.isUpdateReady = HasUpdateFolder(drivePath);
        
        // 设置更新时间
        GetSystemTime(&info.lastUpdate);
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("Failed to get drive information: " + std::string(e.what()));
        return false;
    }
}
#endif

// Linux USB监控实现
#ifdef PLATFORM_LINUX
void USBInfoManager::USBMonitorImpl::MonitorLinuxUSBDevices() {
    try {
        // 获取当前挂载的USB设备
        std::set<std::string> newMountPoints;
        
        // 读取 /proc/mounts 获取挂载点信息
        std::ifstream mounts("/proc/mounts");
        if (!mounts.is_open()) {
            Logger::Error("无法打开 /proc/mounts");
            return;
        }
        
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::string devicePath, mountPoint, fsType;
            iss >> devicePath >> mountPoint >> fsType;
            
            // 检查是否为USB设备 (通常在 /dev/sd* 或 /dev/usb*)
            if ((devicePath.find("/dev/sd") == 0 || devicePath.find("/dev/usb") == 0) &&
                parentManager->IsLinuxUSBDevice(devicePath)) {
                newMountPoints.insert(mountPoint);
            }
        }
        
        // 比较新旧设备列表
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            std::set<std::string> oldMountPoints;
            for (const auto& device : currentDevices) {
                oldMountPoints.insert(device.mountPoint);
            }
            
            // 检测新插入的设备
            for (const auto& mountPoint : newMountPoints) {
                if (oldMountPoints.find(mountPoint) == oldMountPoints.end()) {
                    USBDeviceInfo info;
                    if (parentManager->GetLinuxDeviceInfo(mountPoint, info)) {
                        info.state = USBState::Inserted;
                        info.lastUpdate = time(nullptr);
                        currentDevices.push_back(info);
                        Logger::Info("USB inserted: " + mountPoint + " (" + info.volumeLabel + ")");
                        
                        if (parentManager->stateCallback) {
                            parentManager->stateCallback(info);
                        }
                    }
                }
            }
            
            // 检测拔出的设备
            for (const auto& mountPoint : oldMountPoints) {
                if (newMountPoints.find(mountPoint) == newMountPoints.end()) {
                    for (auto it = currentDevices.begin(); it != currentDevices.end(); ++it) {
                        if (it->mountPoint == mountPoint) {
                            it->state = USBState::Removed;
                            it->lastUpdate = time(nullptr);
                            Logger::Info("USB removed: " + mountPoint + " (" + it->volumeLabel + ")");
                            
                            if (parentManager->stateCallback) {
                                parentManager->stateCallback(*it);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        Logger::Error("MonitorLinuxUSBDevices failed: " + std::string(e.what()));
    }
}

bool USBInfoManager::GetLinuxDeviceInfo(const std::string& mountPoint, USBDeviceInfo& info) {
    try {
        info.mountPoint = mountPoint;
        info.drivePath = mountPoint; // Linux使用挂载点作为驱动路径
        
        // 获取设备路径
        info.devicePath = GetLinuxDevicePath(mountPoint);
        if (info.devicePath.empty()) {
            Logger::Warn("Cannot get device path for mount point: " + mountPoint);
            return false;
        }
        
        // 检查是否为USB设备
        if (!IsLinuxUSBDevice(info.devicePath)) {
            return false;
        }
        
        // 获取设备属性
        GetLinuxDeviceProperties(info.devicePath, info);
        
        // 检查update文件夹
        info.isUpdateReady = HasUpdateFolder(mountPoint);
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxDeviceInfo failed: " + std::string(e.what()));
        return false;
    }
}

std::string USBInfoManager::GetLinuxDevicePath(const std::string& mountPoint) {
    try {
        // 读取 /proc/mounts 获取设备路径
        std::ifstream mounts("/proc/mounts");
        if (!mounts.is_open()) {
            return "";
        }
        
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::string devicePath, mp;
            iss >> devicePath >> mp;
            
            if (mp == mountPoint) {
                return devicePath;
            }
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxDevicePath failed: " + std::string(e.what()));
    }
    
    return "";
}

void USBInfoManager::GetLinuxDeviceProperties(const std::string& devicePath, USBDeviceInfo& info) {
    try {
        // 获取设备详细信息通过udev
        struct udev* udev = udev_new();
        if (!udev) {
            Logger::Error("Failed to create udev context");
            return;
        }
        
        struct udev_device* device = udev_device_new_from_devname(udev, devicePath.c_str());
        if (!device) {
            Logger::Warn("Cannot find udev device for: " + devicePath);
            udev_unref(udev);
            return;
        }
        
        // 获取设备属性
        const char* vendor = udev_device_get_sysattr_value(device, "manufacturer");
        if (vendor) {
            info.vendorId = vendor;
        }
        
        const char* product = udev_device_get_sysattr_value(device, "product");
        if (product) {
            info.volumeLabel = product;
        }
        
        const char* serial = udev_device_get_sysattr_value(device, "serial");
        if (serial) {
            info.serialNumber = serial;
        }
        
        // 获取磁盘空间信息
        struct statvfs vfs;
        if (statvfs(info.mountPoint.c_str(), &vfs) == 0) {
            info.totalSize = vfs.f_blocks * vfs.f_frsize;
            info.freeSpace = vfs.f_bavail * vfs.f_frsize;
            info.usedSpace = info.totalSize - info.freeSpace;
        }
        
        info.isRemovable = true;
        
        udev_device_unref(device);
        udev_unref(udev);
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxDeviceProperties failed: " + std::string(e.what()));
    }
}

bool USBInfoManager::IsLinuxUSBDevice(const std::string& devicePath) {
    try {
        // 检查设备是否为USB设备
        struct udev* udev = udev_new();
        if (!udev) {
            return false;
        }
        
        struct udev_device* device = udev_device_new_from_devname(udev, devicePath.c_str());
        if (!device) {
            udev_unref(udev);
            return false;
        }
        
        // 检查设备的父设备是否为USB设备
        struct udev_device* parent = udev_device_get_parent(device);
        bool isUSB = false;
        
        while (parent && !isUSB) {
            const char* subsystem = udev_device_get_subsystem(parent);
            if (subsystem && std::string(subsystem) == "usb") {
                isUSB = true;
                break;
            }
            parent = udev_device_get_parent(parent);
        }
        
        udev_device_unref(device);
        udev_unref(udev);
        
        return isUSB;
    }
    catch (const std::exception& e) {
        Logger::Error("IsLinuxUSBDevice failed: " + std::string(e.what()));
        return false;
    }
}
#endif

bool USBInfoManager::HasUpdateFolder(const std::string& path) {
    try {
#ifdef PLATFORM_WINDOWS
        std::filesystem::path updatePath = std::filesystem::path(path) / "update";
        if (std::filesystem::exists(updatePath) && std::filesystem::is_directory(updatePath)) {
            // 检查目录是否为空
            auto it = std::filesystem::directory_iterator(updatePath);
            return it != std::filesystem::directory_iterator(); // 不为空返回true
        }
        return false;
#else
        // macOS/Linux实现
        std::string updatePath = path + "/update";
        struct stat st;
        if (stat(updatePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR* dir = opendir(updatePath.c_str());
            if (dir) {
                struct dirent* entry;
                int count = 0;
                while ((entry = readdir(dir)) != nullptr) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        count++;
                        break;
                    }
                }
                closedir(dir);
                return count > 0;
            }
        }
        return false;
#endif
    }

#ifdef PLATFORM_WINDOWS
bool USBInfoManager::IsRemovableDrive(char driveLetter) {
    try {
        std::string rootPath = std::string(1, driveLetter) + ":\\";
        UINT driveType = GetDriveTypeA(rootPath.c_str());
        return driveType == DRIVE_REMOVABLE || driveType == DRIVE_USB;
    }
    catch (const std::exception& e) {
        Logger::Warn("Exception when checking removable drive: " + std::string(e.what()));
        return false;
    }
}
#endif