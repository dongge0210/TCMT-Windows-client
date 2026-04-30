#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Platform macro detection (if not defined in CMake)
#if !defined(TCMT_WINDOWS) && !defined(TCMT_MACOS) && !defined(TCMT_LINUX)
    #if defined(_WIN32) || defined(_WIN64)
        #define TCMT_WINDOWS
    #elif defined(__APPLE__) && defined(__MACH__)
        #define TCMT_MACOS
    #elif defined(__linux__)
        #define TCMT_LINUX
    #endif
#endif

struct UsbDevice {
    std::string name;           // Product name
    std::string manufacturer;   // Vendor name
    uint16_t vid;               // Vendor ID (hex)
    uint16_t pid;               // Product ID (hex)
    std::string serialNumber;
    std::string protocolVersion; // "USB 2.0", "USB 3.1", etc.
    bool isSelfPowered;
    uint32_t maxPower;          // mA
};

struct UsbInfo {
    std::vector<UsbDevice> devices;

    void Detect(); // enumerate all connected USB devices
};
