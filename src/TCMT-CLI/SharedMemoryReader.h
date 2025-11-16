#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../core/DataStruct/DataStruct.h"
#include "../core/common/CrossPlatformStubs.h"

class SharedMemoryReader {
private:
    HANDLE hMapFile;
    void* pBuf;
    bool isConnected;
    std::string lastError;
    
    const std::vector<std::string> memoryNames = {
        "Global\\SystemMonitorSharedMemory",
        "Local\\SystemMonitorSharedMemory", 
        "SystemMonitorSharedMemory"
    };
    
    static const int EXPECTED_SIZE = 3593; // Updated to match current SharedMemoryBlock size

public:
    SharedMemoryReader();
    ~SharedMemoryReader();
    bool Initialize();
    bool ReadSystemInfo(SystemInfo& info);
    bool ValidateLayout();
    void Cleanup();
    std::string GetLastErrorMessage() const;
    bool IsConnected() const;
    std::string GetDiagnosticsInfo() const;

private:
    bool TryConnect(const std::string& name);
    bool ParseSharedMemoryBlock(const SharedMemoryBlock* block, SystemInfo& info);
    void ParseTemperatureSensors(const TemperatureSensor* sensors, int count, SystemInfo& info);
    void ParseSmartDisks(const SmartDiskScore* smartDisks, int count, SystemInfo& info);
    void ParseUSBDevices(const SharedMemoryBlock::USBDeviceData* usbDevices, int count, SystemInfo& info);
    bool WaitForWriteSequenceStable(const SharedMemoryBlock* block);
    std::string ParseUTF8String(const char* data, int maxLength);
    std::string ParseWideString(const wchar_t* data, int maxLength);
    std::string FormatBytes(uint64_t bytes);
    std::string GetCurrentTimestamp();
};