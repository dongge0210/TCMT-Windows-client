#include "BoardInfo.h"
#include "../Utils/Logger.h"

#ifdef TCMT_WINDOWS
// ======================== Windows Implementation ========================
// NOTE: winsock2.h must be included BEFORE windows.h
#include <winsock2.h>
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include "../utils/WinUtils.h"
#include "../utils/WmiManager.h"
#pragma comment(lib, "wbemuuid.lib")

static std::string ChassisTypeToString(uint16_t type) {
    switch (type) {
        case 1:  return "Other";
        case 2:  return "Unknown";
        case 3:  return "Desktop";
        case 4:  return "Low Profile Desktop";
        case 5:  return "Pizza Box";
        case 6:  return "Mini Tower";
        case 7:  return "Tower";
        case 8:  return "Portable";
        case 9:  return "Laptop";
        case 10: return "Notebook";
        case 11: return "Handheld";
        case 12: return "Docking Station";
        case 13: return "All-in-One";
        case 14: return "Sub-Notebook";
        case 15: return "Space-Saving";
        case 16: return "Lunch Box";
        case 17: return "Main Server Chassis";
        case 18: return "Expansion Chassis";
        case 19: return "Sub-Chassis";
        case 20: return "Bus Expansion Chassis";
        case 21: return "Peripheral Chassis";
        case 22: return "Storage Chassis";
        case 23: return "Rack Mount Chassis";
        case 24: return "Sealed-Case PC";
        case 30: return "Tablet";
        case 31: return "Convertible";
        case 32: return "Detachable";
        default: return "Unknown (" + std::to_string(type) + ")";
    }
}

static std::string GetWmiString(IWbemClassObject* obj, const wchar_t* propName) {
    VARIANT vt;
    VariantInit(&vt);
    std::string result;
    if (SUCCEEDED(obj->Get(propName, 0, &vt, 0, 0)) && vt.vt == VT_BSTR) {
        result = WinUtils::WstringToString(std::wstring(vt.bstrVal));
    }
    VariantClear(&vt);
    return result;
}

static std::string ParseWmiDate(const std::string& wmiDate) {
    // WMI datetime format: yyyymmddHHMMSS.xxxxxx+UUU
    if (wmiDate.length() >= 8) {
        return wmiDate.substr(0, 4) + "-"
             + wmiDate.substr(4, 2) + "-"
             + wmiDate.substr(6, 2);
    }
    return wmiDate;
}

void BoardInfo::Detect() {
    WmiManager wmi;
    IWbemServices* svc = wmi.GetWmiService();
    if (!svc) {
        Logger::Warn("BoardInfo: WMI service unavailable");
        return;
    }

    IEnumWbemClassObject* pEnum = nullptr;
    IWbemClassObject* obj = nullptr;
    ULONG ret = 0;

    // --- Win32_BaseBoard ---
    HRESULT hr = svc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT Manufacturer, Product, SerialNumber, Version FROM Win32_BaseBoard"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum
    );
    if (SUCCEEDED(hr) && pEnum) {
        if (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            manufacturer = GetWmiString(obj, L"Manufacturer");
            product = GetWmiString(obj, L"Product");
            serialNumber = GetWmiString(obj, L"SerialNumber");
            version = GetWmiString(obj, L"Version");
            obj->Release();
        }
        pEnum->Release();
        pEnum = nullptr;
    }

    // --- Win32_BIOS ---
    hr = svc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT Manufacturer, Version, ReleaseDate FROM Win32_BIOS"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum
    );
    if (SUCCEEDED(hr) && pEnum) {
        obj = nullptr; ret = 0;
        if (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            biosVendor = GetWmiString(obj, L"Manufacturer");
            biosVersion = GetWmiString(obj, L"Version");
            biosDate = ParseWmiDate(GetWmiString(obj, L"ReleaseDate"));
            obj->Release();
        }
        pEnum->Release();
        pEnum = nullptr;
    }

    // --- Win32_ComputerSystemProduct ---
    hr = svc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT UUID FROM Win32_ComputerSystemProduct"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum
    );
    if (SUCCEEDED(hr) && pEnum) {
        obj = nullptr; ret = 0;
        if (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            systemUuid = GetWmiString(obj, L"UUID");
            obj->Release();
        }
        pEnum->Release();
        pEnum = nullptr;
    }

    // --- Win32_SystemEnclosure ---
    hr = svc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT ChassisTypes FROM Win32_SystemEnclosure"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum
    );
    if (SUCCEEDED(hr) && pEnum) {
        obj = nullptr; ret = 0;
        if (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            VARIANT vt;
            VariantInit(&vt);
            if (SUCCEEDED(obj->Get(L"ChassisTypes", 0, &vt, 0, 0))) {
                // ChassisTypes is uint16[] — take the first element
                if ((vt.vt & VT_ARRAY) && vt.parray) {
                    long lower = 0, upper = 0;
                    if (SafeArrayGetLBound(vt.parray, 1, &lower) == S_OK &&
                        SafeArrayGetUBound(vt.parray, 1, &upper) == S_OK &&
                        lower <= upper) {
                        unsigned short value = 0;
                        SafeArrayGetElement(vt.parray, &lower, &value);
                        chassisType = ChassisTypeToString(value);
                    }
                } else if (vt.vt == VT_UI2 || vt.vt == VT_I4 || vt.vt == VT_UI4) {
                    // Fallback for systems that return a scalar
                    uint16_t val = 0;
                    if (vt.vt == VT_UI2) val = vt.uiVal;
                    else if (vt.vt == VT_I4) val = static_cast<uint16_t>(vt.intVal);
                    else if (vt.vt == VT_UI4) val = static_cast<uint16_t>(vt.uintVal);
                    chassisType = ChassisTypeToString(val);
                }
            }
            VariantClear(&vt);
            obj->Release();
        }
        pEnum->Release();
    }
}

#elif defined(TCMT_MACOS)
// ======================== macOS Implementation ========================
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>

void BoardInfo::Detect() {
    // --- IOKit: IOPlatformExpertDevice ---
    mach_port_t masterPort = 0;
    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr != KERN_SUCCESS) {
        Logger::Warn("BoardInfo: IOMasterPort failed");
        return;
    }

    CFMutableDictionaryRef matching = IOServiceMatching("IOPlatformExpertDevice");
    if (!matching) return;

    io_service_t platformExpert = IOServiceGetMatchingService(masterPort, matching);
    if (!platformExpert) {
        Logger::Warn("BoardInfo: no IOPlatformExpertDevice found");
        return;
    }

    // Helper: read a CFString property from the registry entry
    auto readStringProp = [&](const char* key) -> std::string {
        CFStringRef cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
        if (!cfKey) return {};

        CFTypeRef value = IORegistryEntryCreateCFProperty(platformExpert, cfKey, kCFAllocatorDefault, 0);
        CFRelease(cfKey);
        if (!value) return {};

        std::string result;
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            char buf[256] = {0};
            if (CFStringGetCString((CFStringRef)value, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                result = buf;
            }
        }
        CFRelease(value);
        return result;
    };

    manufacturer = readStringProp("manufacturer");
    product = readStringProp("product-name");
    serialNumber = readStringProp("serial-number");
    systemUuid = readStringProp("UUID");
    version = readStringProp("version");

    // Fallback to well-known IORegistry keys if the above returned empty
    if (serialNumber.empty()) serialNumber = readStringProp("IOPlatformSerialNumber");
    if (systemUuid.empty())   systemUuid   = readStringProp("IOPlatformUUID");
    if (product.empty())      product      = readStringProp("model");

    IOObjectRelease(platformExpert);

    // --- sysctl for DMI data (Intel Macs; silently fail on Apple Silicon) ---
    char buf[256] = {0};
    size_t len = sizeof(buf);

    if (sysctlbyname("machdep.dmi.bios-vendor", buf, &len, nullptr, 0) == 0) {
        biosVendor = buf;
    }
    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    if (sysctlbyname("machdep.dmi.bios-version", buf, &len, nullptr, 0) == 0) {
        biosVersion = buf;
    }
    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    if (sysctlbyname("machdep.dmi.bios-date", buf, &len, nullptr, 0) == 0) {
        biosDate = buf;
    }
}

#else
#error "Unsupported platform"
#endif
