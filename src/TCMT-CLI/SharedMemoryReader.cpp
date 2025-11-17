#include "SharedMemoryReader.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>
#include <ctime>

#ifdef PLATFORM_WINDOWS
    #include "../core/common/CrossPlatformCompat.h"
#else
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <unistd.h>
    #include <errno.h>
#endif

#include "../core/Utils/Logger.h"

SharedMemoryReader::SharedMemoryReader() 
    : hMapFile(0), pBuf(nullptr), isConnected(false) {
}

SharedMemoryReader::~SharedMemoryReader() {
    Cleanup();
}

bool SharedMemoryReader::Initialize() {
    if (isConnected) {
        return true;
    }
    
    lastError.clear();
    
    
    for (const auto& name : memoryNames) {
        if (TryConnect(name)) {
            isConnected = true;
            std::cout << "Connected to shared memory: " << name << std::endl;
            return true;
        }
    }

    lastError = "Shared memory not found (backend not running or name mismatch)";
    std::cerr << lastError << std::endl;
    return false;
}

bool SharedMemoryReader::TryConnect(const std::string& name) {
#ifdef PLATFORM_WINDOWS
    hMapFile = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        name.c_str()
    );
    
    if (hMapFile == nullptr) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            std::cerr << "Failed to open shared memory " << name << ", error: " << error << std::endl;
        }
        return false;
    }
    
    
    pBuf = MapViewOfFile(
        hMapFile,
        FILE_MAP_READ,
        0,
        0,
#else
    // macOS/Linux POSIX shared memory implementation
    std::string shmName = "/" + name;
    int shmFd = shm_open(shmName.c_str(), O_RDONLY, 0666);
    if (shmFd == -1) {
        std::cerr << "Failed to open shared memory " << name << std::endl;
        return false;
    }
    
    pBuf = mmap(nullptr, sizeof(SharedMemoryBlock), PROT_READ, MAP_SHARED, shmFd, 0);
    if (pBuf == MAP_FAILED) {
        close(shmFd);
        std::cerr << "Failed to map shared memory " << name << std::endl;
        return false;
    }
    
    hMapFile = shmFd; // Store file descriptor for cleanup
#endif
    
    // Verify shared memory block size
    if (sizeof(SharedMemoryBlock) != EXPECTED_SIZE) {
        Logger::Error("SharedMemoryBlock size mismatch: expected " + 
                     std::to_string(EXPECTED_SIZE) + 
                     ", actual " + std::to_string(sizeof(SharedMemoryBlock)));
        return false;
    }
    
    if (pBuf == nullptr) {
#ifdef PLATFORM_WINDOWS
        DWORD error = GetLastError();
        std::cerr << "Failed to map shared memory " << name << ", error: " << error << std::endl;
        CloseHandle(hMapFile);
        hMapFile = 0;
#else
        std::cerr << "Failed to map shared memory " << name << std::endl;
        close(hMapFile);
        hMapFile = -1;
#endif
        return false;
    }
    
    return true;
}

bool SharedMemoryReader::ReadSystemInfo(SystemInfo& info) {
    if (!isConnected && !Initialize()) {
        return false;
    }
    
    if (pBuf == nullptr) {
        lastError = "Shared memory buffer is invalid";
        return false;
    }
    
    try {
        const SharedMemoryBlock* block = static_cast<const SharedMemoryBlock*>(pBuf);
        
        
        if (!WaitForWriteSequenceStable(block)) {
            lastError = "Timeout waiting for stable write sequence";
            return false;
        }
        
        
        return ParseSharedMemoryBlock(block, info);
    }
    catch (const std::exception& e) {
        lastError = "Exception while reading system info: " + std::string(e.what());
        std::cerr << lastError << std::endl;
        return false;
    }
}

bool SharedMemoryReader::WaitForWriteSequenceStable(const SharedMemoryBlock* block) {
    if (block == nullptr) {
        return false;
    }
    
    const int MAX_RETRIES = 10;
    const int RETRY_DELAY_MS = 5;
    
    for (int i = 0; i < MAX_RETRIES; ++i) {
        uint32_t sequence = block->writeSequence;
        
        
        if ((sequence & 1) == 0) {
            return true;
        }
        
        
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
    }
    
    return false;
}

bool SharedMemoryReader::ParseSharedMemoryBlock(const SharedMemoryBlock* block, SystemInfo& info) {
    if (block == nullptr) {
        return false;
    }
    
    try {

        info = SystemInfo();


        info.physicalCores = block->cpuLogicalCores;
        info.logicalCores = block->cpuLogicalCores;
        info.cpuUsage = (block->cpuUsagePercent_x10 >= 0) ? 
            (block->cpuUsagePercent_x10 / 10.0) : 0.0;
        
        
        info.totalMemory = block->memoryTotalMB * 1024ULL * 1024ULL;
        info.usedMemory = block->memoryUsedMB * 1024ULL * 1024ULL;
        info.availableMemory = (info.totalMemory > info.usedMemory) ? 
            (info.totalMemory - info.usedMemory) : 0ULL;
        
        
        std::ostringstream oss;
        oss << "SystemMonitor ABI: 0x" << std::hex << std::uppercase << block->abiVersion;
        info.cpuName = oss.str();
        
        
        ParseTemperatureSensors(block->tempSensors, block->tempSensorCount, info);


        ParseSmartDisks(block->smartDisks, block->smartDiskCount, info);


        ParseUSBDevices(block->usbDevices, block->usbDeviceCount, info);


        // Cross-platform time update
#ifdef PLATFORM_WINDOWS
        GetSystemTime(&info.lastUpdate.windowsTime);
#else
        // For non-Windows platforms, use current time
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        info.lastUpdate.unixTime = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        info.lastUpdate.milliseconds = millis.count() % 1000;
#endif

        return true;
    }
    catch (const std::exception& e) {
        lastError = "Exception while parsing shared memory block: " + std::string(e.what());
        return false;
    }
}

void SharedMemoryReader::ParseTemperatureSensors(const TemperatureSensor* sensors, int count, SystemInfo& info) {
    if (sensors == nullptr || count <= 0) {
        return;
    }
    
    info.temperatures.clear();
    
    int maxCount = std::min(count, 32);
    
    for (int i = 0; i < maxCount; ++i) {
        const TemperatureSensor& sensor = sensors[i];
        
        
    std::string name = ParseUTF8String(sensor.name, 32);
        if (name.empty()) {
            name = "Temp" + std::to_string(i);
        }
        
        
        double temperature = (sensor.valueC_x10 >= 0) ? 
            (sensor.valueC_x10 / 10.0) : -999.0;
        
        
        info.temperatures.emplace_back(name, temperature);
        
        
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), 
                      [](unsigned char c){ return std::tolower(c); });
        
        if (lowerName.find("cpu") != std::string::npos && info.cpuTemperature <= -100.0) {
            info.cpuTemperature = temperature;
        }
        
        if ((lowerName.find("gpu") != std::string::npos || 
             lowerName.find("graphics") != std::string::npos) && 
            info.gpuTemperature <= -100.0) {
            info.gpuTemperature = temperature;
        }
    }
}

void SharedMemoryReader::ParseSmartDisks(const SmartDiskScore* smartDisks, int count, SystemInfo& info) {
    if (smartDisks == nullptr || count <= 0) {
        return;
    }
    
    int maxCount = std::min(count, 16);
    
    for (int i = 0; i < maxCount; ++i) {
        const SmartDiskScore& disk = smartDisks[i];
        
        
        std::string diskId = ParseUTF8String(disk.diskId, 32);
        if (diskId.empty()) {
            continue;
        }
        
        
    }
}

void SharedMemoryReader::ParseUSBDevices(const SharedMemoryBlock::USBDeviceData* usbDevices, int count, SystemInfo& info) {
    if (usbDevices == nullptr || count <= 0) {
        return;
    }
    
    info.usbDevices.clear();
    
    int maxCount = std::min(count, 8);
    
    for (int i = 0; i < maxCount; ++i) {
        const SharedMemoryBlock::USBDeviceData& device = usbDevices[i];
        
        
        std::string drivePath = ParseUTF8String(device.drivePath, 4);
        if (drivePath.empty()) {
            continue;
        }
        
        
        USBDeviceInfo usbInfo;
        usbInfo.drivePath = drivePath;
        usbInfo.volumeLabel = ParseUTF8String(device.volumeLabel, 32);
        usbInfo.totalSize = device.totalSize;
        usbInfo.freeSpace = device.freeSpace;
        usbInfo.isUpdateReady = (device.isUpdateReady != 0);
        usbInfo.state = static_cast<USBState>(device.state);
        // Cross-platform time assignment
#ifdef PLATFORM_WINDOWS
        usbInfo.lastUpdate = device.lastUpdate.windowsTime;
#else
        usbInfo.lastUpdate = device.lastUpdate.unixTime;
#endif
        
        info.usbDevices.push_back(usbInfo);
    }
}

std::string SharedMemoryReader::ParseUTF8String(const char* data, int maxLength) {
    if (data == nullptr || maxLength <= 0) {
        return "";
    }
    
    
    int length = 0;
    while (length < maxLength && data[length] != '\0') {
        ++length;
    }
    
    if (length == 0) {
        return "";
    }
    
    try {
        return std::string(data, length);
    }
    catch (const std::exception&) {
        return "";
    }
}

std::string SharedMemoryReader::ParseWideString(const wchar_t* data, int maxLength) {
    if (data == nullptr || maxLength <= 0) {
        return "";
    }
    
    
    int length = 0;
    while (length < maxLength && data[length] != L'\0') {
        ++length;
    }
    
    if (length == 0) {
        return "";
    }
    
    try {
        
        std::wstring wideStr(data, length);
        std::string utf8Str;
        utf8Str.reserve(wideStr.length());
        
        for (wchar_t wc : wideStr) {
            if (wc < 0x80) {
                utf8Str.push_back(static_cast<char>(wc));
            } else if (wc < 0x800) {
                utf8Str.push_back(static_cast<char>(0xC0 | (wc >> 6)));
                utf8Str.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            } else {
                utf8Str.push_back(static_cast<char>(0xE0 | (wc >> 12)));
                utf8Str.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                utf8Str.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
        }
        
        return utf8Str;
    }
    catch (const std::exception&) {
        return "";
    }
}

bool SharedMemoryReader::ValidateLayout() {
    if (!isConnected && !Initialize()) {
        return false;
    }
    
    if (pBuf == nullptr) {
        return false;
    }
    
    try {
        const SharedMemoryBlock* block = static_cast<const SharedMemoryBlock*>(pBuf);
        
        
        if (block->abiVersion != 0x00010014) {
            lastError = "ABI version mismatch, expected: 0x00010014, actual: 0x" + 
                std::to_string(block->abiVersion);
            return false;
        }


        if (block->tempSensorCount > 32) {
            lastError = "Temperature sensor count abnormal: " + std::to_string(block->tempSensorCount);
            return false;
        }


        if (block->smartDiskCount > 16) {
            lastError = "SMART disk count abnormal: " + std::to_string(block->smartDiskCount);
            return false;
        }


        if (block->usbDeviceCount > 8) {
            lastError = "USB device count abnormal: " + std::to_string(block->usbDeviceCount);
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        lastError = "Exception during layout validation: " + std::string(e.what());
        return false;
    }
}

void SharedMemoryReader::Cleanup() {
    if (pBuf != nullptr) {
#ifdef PLATFORM_WINDOWS
        UnmapViewOfFile(pBuf);
#else
        munmap(pBuf, sizeof(SharedMemoryBlock));
#endif
        pBuf = nullptr;
    }
    
    if (hMapFile != 0) {
#ifdef PLATFORM_WINDOWS
        CloseHandle(hMapFile);
#else
        close(hMapFile);
#endif
        hMapFile = 0;
    }
    
    isConnected = false;
}

std::string SharedMemoryReader::GetLastErrorMessage() const {
    return lastError;
}

bool SharedMemoryReader::IsConnected() const {
    return isConnected;
}

std::string SharedMemoryReader::GetDiagnosticsInfo() const {
    if (!isConnected || pBuf == nullptr) {
        return "Not connected to shared memory";
    }
    
    try {
        const SharedMemoryBlock* block = static_cast<const SharedMemoryBlock*>(pBuf);
        
    std::ostringstream oss;
    oss << "Shared memory diagnostics:\n";
    oss << "ABI version: 0x" << std::hex << std::uppercase << block->abiVersion << "\n";
    oss << "Write sequence: " << std::dec << block->writeSequence << "\n";
    oss << "Snapshot version: " << block->snapshotVersion << "\n";
    oss << "CPU logical cores: " << block->cpuLogicalCores << "\n";
    oss << "CPU usage (x10): " << block->cpuUsagePercent_x10 << "\n";
    oss << "Memory total (MB): " << block->memoryTotalMB << "\n";
    oss << "Memory used (MB): " << block->memoryUsedMB << "\n";
    oss << "Temperature sensor count: " << block->tempSensorCount << "\n";
    oss << "SMART disk count: " << static_cast<int>(block->smartDiskCount) << "\n";
    oss << "USB device count: " << static_cast<int>(block->usbDeviceCount) << "\n";
    oss << "TPM present: " << (block->tpmPresent ? "yes" : "no") << "\n";
    oss << "Secure boot enabled: " << (block->secureBootEnabled ? "yes" : "no") << "\n";
        
        return oss.str();
    }
    catch (const std::exception& e) {
        return "Exception getting diagnostics info: " + std::string(e.what());
    }
}

std::string SharedMemoryReader::FormatBytes(uint64_t bytes) {
    if (bytes == 0) {
        return "0 B";
    }
    
    const uint64_t KB = 1024ULL;
    const uint64_t MB = KB * KB;
    const uint64_t GB = MB * KB;
    const uint64_t TB = GB * KB;
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    
    if (bytes >= TB) {
        oss << (static_cast<double>(bytes) / TB) << " TB";
    } else if (bytes >= GB) {
        oss << (static_cast<double>(bytes) / GB) << " GB";
    } else if (bytes >= MB) {
        oss << (static_cast<double>(bytes) / MB) << " MB";
    } else if (bytes >= KB) {
        oss << (static_cast<double>(bytes) / KB) << " KB";
    } else {
        oss << bytes << " B";
    }
    
    return oss.str();
}

std::string SharedMemoryReader::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto raw_time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    struct tm timeinfo;
#ifdef PLATFORM_WINDOWS
    localtime_s(&timeinfo, &raw_time);
#else
    localtime_r(&raw_time, &timeinfo);
#endif
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}