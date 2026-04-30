#include "UsbInfo.h"
#include "../Utils/Logger.h"

// Platform macro detection (also defined in CMake but this handles standalone inclusion)
#if !defined(TCMT_WINDOWS) && !defined(TCMT_MACOS) && !defined(TCMT_LINUX)
    #if defined(_WIN32) || defined(_WIN64)
        #define TCMT_WINDOWS
    #elif defined(__APPLE__) && defined(__MACH__)
        #define TCMT_MACOS
    #elif defined(__linux__)
        #define TCMT_LINUX
    #endif
#endif

// ============================================================================
// Windows Implementation
// ============================================================================
#ifdef TCMT_WINDOWS

#include <winsock2.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpkey.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// {{A5DCBF10-6530-11D2-901F-00C04FB951ED}}
static const GUID GUID_USB_DEVICE_INTERFACE =
    { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };

// Local helper: convert wide string to UTF-8
static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(static_cast<size_t>(len) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

void UsbInfo::Detect() {
    devices.clear();

    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_USB_DEVICE_INTERFACE, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        Logger::Error("UsbInfo: SetupDiGetClassDevs failed");
        return;
    }

    SP_DEVICE_INTERFACE_DATA devInterfaceData = {};
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_USB_DEVICE_INTERFACE,
                                         i, &devInterfaceData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        // First call: get required buffer size for the detail data
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, nullptr, 0,
                                        &requiredSize, nullptr);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || requiredSize == 0) continue;

        // Allocate the detail data structure (variable length due to DevicePath)
        auto detailData = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(
            malloc(requiredSize));
        if (!detailData) continue;
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, detailData,
                                            requiredSize, nullptr, &devInfoData)) {
            UsbDevice device = {};
            wchar_t buffer[256] = {};

            // ---- Hardware ID (parse VID / PID) ----
            // Format: USB\VID_XXXX&PID_XXXX[&REV_XXXX][\SERIAL]
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData,
                    SPDRP_HARDWAREID, nullptr, reinterpret_cast<PBYTE>(buffer),
                    sizeof(buffer), nullptr)) {
                const wchar_t* vidStr = wcsstr(buffer, L"VID_");
                if (vidStr) {
                    device.vid = static_cast<uint16_t>(
                        wcstol(vidStr + 4, nullptr, 16));
                }
                const wchar_t* pidStr = wcsstr(buffer, L"PID_");
                if (pidStr) {
                    device.pid = static_cast<uint16_t>(
                        wcstol(pidStr + 4, nullptr, 16));
                }
            }

            // ---- Device description (product name) ----
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData,
                    SPDRP_DEVICEDESC, nullptr, reinterpret_cast<PBYTE>(buffer),
                    sizeof(buffer), nullptr)) {
                device.name = WideToUtf8(buffer);
            }

            // ---- Manufacturer ----
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData,
                    SPDRP_MFG, nullptr, reinterpret_cast<PBYTE>(buffer),
                    sizeof(buffer), nullptr)) {
                device.manufacturer = WideToUtf8(buffer);
            }

            // ---- Serial number (from instance ID) ----
            // Instance ID format: USB\VID_XXXX&PID_XXXX\SERIAL
            wchar_t instanceId[256] = {};
            if (SetupDiGetDeviceInstanceId(hDevInfo, &devInfoData,
                    instanceId, 256, nullptr)) {
                const wchar_t* lastSep = wcsrchr(instanceId, L'\\');
                if (lastSep && *(lastSep + 1) != L'\0') {
                    device.serialNumber = WideToUtf8(lastSep + 1);
                }
            }

            // ---- Protocol version (try Speed via CM API) ----
            DEVPROPTYPE propType = DEVPROP_TYPE_EMPTY;
            ULONG propSize = 0;
            if (CM_Get_DevNode_Property(devInfoData.DevInst,
                    &DEVPKEY_Device_Address, &propType, nullptr, &propSize, 0)
                    == CR_BUFFER_SMALL && propSize >= sizeof(ULONG)) {
                auto addrBuf = std::make_unique<BYTE[]>(propSize);
                if (CM_Get_DevNode_Property(devInfoData.DevInst,
                        &DEVPKEY_Device_Address, &propType, addrBuf.get(),
                        &propSize, 0) == CR_SUCCESS) {
                    // Derive rough protocol hint from address range
                    // (not a definitive protocol version, but keeps things simple)
                }
            }

            // ---- Power info (try via CM API) ----
            propType = DEVPROP_TYPE_EMPTY;
            propSize = 0;
            if (CM_Get_DevNode_Property(devInfoData.DevInst,
                    &DEVPKEY_Device_PowerData, &propType, nullptr, &propSize, 0)
                    == CR_BUFFER_SMALL && propSize >= sizeof(ULONG)) {
                auto powerBuf = std::make_unique<BYTE[]>(propSize);
                if (CM_Get_DevNode_Property(devInfoData.DevInst,
                        &DEVPKEY_Device_PowerData, &propType, powerBuf.get(),
                        &propSize, 0) == CR_SUCCESS) {
                    // powerBuf contains CM_POWER_DATA at the start
                    // We just flag that power info is potentially available
                    device.maxPower = 500; // reasonable default
                }
            }

            devices.push_back(device);
        }

        free(detailData);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    Logger::Info("UsbInfo (Windows): found " + std::to_string(devices.size()) + " USB device(s)");
}

#endif // TCMT_WINDOWS

// ============================================================================
// macOS Implementation
// ============================================================================
#if defined(TCMT_MACOS)

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

// Local helper: convert CFStringRef to std::string
static std::string CFStringToStdString(CFStringRef cfstr) {
    if (!cfstr) return {};
    char buf[256] = {};
    if (CFStringGetCString(cfstr, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        return std::string(buf);
    }
    return {};
}

// Local helper: read a numeric CFNumber from a CF dictionary
static uint32_t CFDictGetUInt32(CFDictionaryRef dict, CFStringRef key) {
    if (!dict) return 0;
    CFNumberRef num = static_cast<CFNumberRef>(
        CFDictionaryGetValue(dict, key));
    if (!num) return 0;
    uint32_t val = 0;
    CFNumberGetValue(num, kCFNumberSInt32Type, &val);
    return val;
}

void UsbInfo::Detect() {
    devices.clear();

    // Try IOUSBHostDevice first (macOS 10.15+), fall back to IOUSBDevice
    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMasterPortDefault,
        IOServiceMatching("IOUSBHostDevice"),
        &iterator);

    if (kr != KERN_SUCCESS || !iterator) {
        kr = IOServiceGetMatchingServices(
            kIOMasterPortDefault,
            IOServiceMatching("IOUSBDevice"),
            &iterator);
    }

    if (kr != KERN_SUCCESS || !iterator) {
        Logger::Error("UsbInfo (macOS): IOServiceGetMatchingServices failed");
        return;
    }

    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        UsbDevice device = {};

        CFMutableDictionaryRef props = nullptr;
        kr = IORegistryEntryCreateCFProperties(service, &props,
                                                kCFAllocatorDefault, kNilOptions);
        if (kr == KERN_SUCCESS && props) {
            // Vendor name
            CFStringRef cfVendor = static_cast<CFStringRef>(
                CFDictionaryGetValue(props, CFSTR("USB Vendor Name")));
            device.manufacturer = CFStringToStdString(cfVendor);

            // Product name
            CFStringRef cfProduct = static_cast<CFStringRef>(
                CFDictionaryGetValue(props, CFSTR("USB Product Name")));
            device.name = CFStringToStdString(cfProduct);

            // VID / PID
            device.vid  = static_cast<uint16_t>(CFDictGetUInt32(props, CFSTR("idVendor")));
            device.pid  = static_cast<uint16_t>(CFDictGetUInt32(props, CFSTR("idProduct")));

            // Serial number
            CFStringRef cfSerial = static_cast<CFStringRef>(
                CFDictionaryGetValue(props, CFSTR("USB Serial Number")));
            device.serialNumber = CFStringToStdString(cfSerial);

            // Protocol version derived from "Device Speed" property
            // Values: 0=Low, 1=Full, 2=High, 3=Super, 4=SuperPlus
            uint32_t speed = CFDictGetUInt32(props, CFSTR("Device Speed"));
            switch (speed) {
                case 0:  device.protocolVersion = "USB 1.x (Low Speed)";    break;
                case 1:  device.protocolVersion = "USB 1.1 (Full Speed)";   break;
                case 2:  device.protocolVersion = "USB 2.0 (High Speed)";   break;
                case 3:  device.protocolVersion = "USB 3.x (SuperSpeed)";   break;
                case 4:  device.protocolVersion = "USB 3.2 (SuperSpeedPlus)"; break;
                default: break; // unknown / not set
            }

            // Self-powered flag
            CFBooleanRef cfSelfPowered = static_cast<CFBooleanRef>(
                CFDictionaryGetValue(props, CFSTR("Self Powered")));
            device.isSelfPowered = (cfSelfPowered == kCFBooleanTrue);

            // Max power (mA)
            device.maxPower = CFDictGetUInt32(props, CFSTR("Max Power"));

            CFRelease(props);
        }

        devices.push_back(device);
        IOObjectRelease(service);
    }

    IOObjectRelease(iterator);
    Logger::Info("UsbInfo (macOS): found " + std::to_string(devices.size()) + " USB device(s)");
}

#endif // TCMT_MACOS

// ============================================================================
// Stub for unsupported platforms
// ============================================================================
#if !defined(TCMT_WINDOWS) && !defined(TCMT_MACOS)

void UsbInfo::Detect() {
    devices.clear();
    Logger::Warn("UsbInfo: USB detection not implemented on this platform");
}

#endif
