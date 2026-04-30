#include "PowerInfo.h"
#include "../Utils/Logger.h"

#ifdef TCMT_WINDOWS
// ======================== Windows Implementation ========================
// NOTE: winsock2.h must be included BEFORE windows.h
#include <winsock2.h>
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <pdh.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "pdh.lib")

// Helper: execute a WQL query and return the enumerator
static IEnumWbemClassObject* WmiQuery(IWbemServices* svc, const wchar_t* wql) {
    IEnumWbemClassObject* enumerator = nullptr;
    svc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(wql),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &enumerator
    );
    return enumerator;
}

// Helper: read a string property from a WMI object
static bool WmiGetString(IWbemClassObject* obj, const wchar_t* prop, std::wstring& out) {
    VARIANT vt;
    VariantInit(&vt);
    HRESULT hr = obj->Get(prop, 0, &vt, 0, 0);
    if (SUCCEEDED(hr) && vt.vt == VT_BSTR) {
        out = vt.bstrVal;
        VariantClear(&vt);
        return true;
    }
    VariantClear(&vt);
    return false;
}

// Helper: read a uint32 property from a WMI object
static bool WmiGetUint32(IWbemClassObject* obj, const wchar_t* prop, uint32_t& out) {
    VARIANT vt;
    VariantInit(&vt);
    HRESULT hr = obj->Get(prop, 0, &vt, 0, 0);
    if (SUCCEEDED(hr)) {
        if (vt.vt == VT_UI4) {
            out = vt.uintVal;
            VariantClear(&vt);
            return true;
        } else if (vt.vt == VT_I4 && vt.intVal >= 0) {
            out = static_cast<uint32_t>(vt.intVal);
            VariantClear(&vt);
            return true;
        }
    }
    VariantClear(&vt);
    return false;
}

// Helper: read a uint16 property from a WMI object
static bool WmiGetUint16(IWbemClassObject* obj, const wchar_t* prop, uint16_t& out) {
    VARIANT vt;
    VariantInit(&vt);
    HRESULT hr = obj->Get(prop, 0, &vt, 0, 0);
    if (SUCCEEDED(hr)) {
        if (vt.vt == VT_UI2) {
            out = vt.uiVal;
            VariantClear(&vt);
            return true;
        } else if (vt.vt == VT_UI4) {
            out = static_cast<uint16_t>(vt.uintVal);
            VariantClear(&vt);
            return true;
        } else if (vt.vt == VT_I4 && vt.intVal >= 0) {
            out = static_cast<uint16_t>(vt.intVal);
            VariantClear(&vt);
            return true;
        }
    }
    VariantClear(&vt);
    return false;
}

void PowerInfo::Detect() {
    batteries.clear();
    isAcPlugged = false;
    systemPowerWatts = 0.0;

    // --- AC status via GetSystemPowerStatus ---
    SYSTEM_POWER_STATUS sysPowerStatus;
    if (GetSystemPowerStatus(&sysPowerStatus)) {
        isAcPlugged = (sysPowerStatus.ACLineStatus == 1);
    }

    // --- Initialize COM for WMI queries ---
    bool comInitialized = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        comInitialized = true;
    } else if (hr == RPC_E_CHANGED_MODE) {
        // COM was already initialized in a different mode — we can still use it
        // but must not call CoUninitialize later
        comInitialized = false;
    } else {
        Logger::Error("PowerInfo: CoInitializeEx failed (0x" +
                      std::to_string(static_cast<uint32_t>(hr)) + ")");
    }

    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;

    if (comInitialized) {
        hr = CoCreateInstance(
            CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<void**>(&pLoc));
        if (FAILED(hr)) {
            Logger::Error("PowerInfo: CoCreateInstance(WbemLocator) failed");
            CoUninitialize();
            comInitialized = false;
        }
    }

    if (pLoc) {
        hr = pLoc->ConnectServer(
            bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
        if (FAILED(hr)) {
            Logger::Error("PowerInfo: ConnectServer(ROOT\\CIMV2) failed");
            pLoc->Release();
            pLoc = nullptr;
            CoUninitialize();
            comInitialized = false;
        }
    }

    if (pSvc) {
        // Set WMI security
        CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr, EOAC_NONE);

        // --- Power Plan via Win32_PowerPlan ---
        IEnumWbemClassObject* enumPlan = WmiQuery(
            pSvc, L"SELECT ElementName FROM Win32_PowerPlan WHERE IsActive = TRUE");
        if (enumPlan) {
            IWbemClassObject* obj = nullptr;
            ULONG ret = 0;
            if (enumPlan->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
                std::wstring planName;
                if (WmiGetString(obj, L"ElementName", planName)) {
                    // Convert wide string to UTF-8
                    int len = WideCharToMultiByte(CP_UTF8, 0, planName.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        powerPlan.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, planName.c_str(), -1, &powerPlan[0], len, nullptr, nullptr);
                    }
                }
                obj->Release();
            }
            enumPlan->Release();
        }

        // --- Battery via Win32_Battery ---
        IEnumWbemClassObject* enumBat = WmiQuery(
            pSvc, L"SELECT Name, DesignCapacity, FullChargeCapacity, "
                  L"EstimatedChargeRemaining, BatteryStatus, DesignVoltage FROM Win32_Battery");
        if (enumBat) {
            IWbemClassObject* obj = nullptr;
            ULONG ret = 0;
            while (enumBat->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
                BatteryInfo bat;
                std::wstring wname;
                if (WmiGetString(obj, L"Name", wname)) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        bat.name.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &bat.name[0], len, nullptr, nullptr);
                    }
                }
                uint32_t desCap = 0, fullCap = 0, desVolt = 0;
                uint16_t batStatus = 0, estCharge = 0;
                WmiGetUint32(obj, L"DesignCapacity", desCap);
                WmiGetUint32(obj, L"FullChargeCapacity", fullCap);
                WmiGetUint32(obj, L"DesignVoltage", desVolt);
                WmiGetUint16(obj, L"BatteryStatus", batStatus);
                WmiGetUint16(obj, L"EstimatedChargeRemaining", estCharge);

                bat.designCapacity = desCap;      // mWh
                bat.fullChargeCapacity = fullCap; // mWh
                bat.voltage = desVolt;             // mV

                // Compute currentCapacity from percentage if possible
                if (fullCap > 0 && estCharge > 0) {
                    bat.currentCapacity = static_cast<uint64_t>(fullCap) * estCharge / 100;
                } else {
                    bat.currentCapacity = 0;
                }

                // Charge percentage from WMI
                if (estCharge <= 100) {
                    bat.chargePercent = static_cast<double>(estCharge);
                }

                // Wear level
                if (bat.designCapacity > 0) {
                    bat.wearLevel = 100.0 * (1.0 - static_cast<double>(bat.fullChargeCapacity)
                                              / static_cast<double>(bat.designCapacity));
                    if (bat.wearLevel < 0.0) bat.wearLevel = 0.0;
                }

                // BatteryStatus: 1=discharging, 2=AC, 3=full, 6=charging
                bat.isCharging = (batStatus == 6 || batStatus == 7 || batStatus == 8 || batStatus == 9);
                bat.isAcConnected = isAcPlugged;

                // timeRemaining from GetSystemPowerStatus (seconds -> minutes)
                if (sysPowerStatus.BatteryLifeTime != (DWORD)-1) {
                    bat.timeRemaining = sysPowerStatus.BatteryLifeTime / 60;
                } else {
                    bat.timeRemaining = 0;
                }

                batteries.push_back(std::move(bat));
                obj->Release();
            }
            enumBat->Release();
        }

        pSvc->Release();
        pLoc->Release();
    }

    if (comInitialized) {
        CoUninitialize();
    }

    // --- Rough system power estimate via PDH ---
    HQUERY hQuery = nullptr;
    HCOUNTER hCounter = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &hQuery) == ERROR_SUCCESS) {
        if (PdhAddEnglishCounterW(hQuery, L"\\Processor(_Total)\\% Processor Performance",
                                  0, &hCounter) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE counterVal;
            if (PdhCollectQueryData(hQuery) == ERROR_SUCCESS) {
                // Brief sleep to allow PDH to gather a delta sample
                Sleep(250);
                if (PdhCollectQueryData(hQuery) == ERROR_SUCCESS) {
                    if (PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE,
                                                    nullptr, &counterVal) == ERROR_SUCCESS) {
                        // The counter represents percentage of maximum processor performance.
                        // Use it as a rough linear scale: 100% = typical max ~TDP watts.
                        // This is a very rough estimate; leave as 0 on failure.
                        double perfPct = counterVal.doubleValue;
                        if (perfPct > 0.0) {
                            // Rough power: assume TDP ~65W typical desktop, scale linearly
                            systemPowerWatts = 65.0 * (perfPct / 100.0);
                        }
                    }
                }
            }
        }
        PdhCloseQuery(hQuery);
    }

    Logger::Info("PowerInfo: detection complete (" +
                 std::to_string(batteries.size()) + " batteries, AC=" +
                 std::to_string(isAcPlugged) + ")");
}

#elif defined(TCMT_MACOS)
// ======================== macOS Implementation ========================
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

// Helper: read a CFNumber as a specific integer type
template <typename T>
static bool CfNumberGetValue(CFNumberRef num, T& out) {
    if (!num) return false;
    if constexpr (std::is_same_v<T, uint64_t>) {
        return CFNumberGetValue(num, kCFNumberSInt64Type, &out) != 0;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return CFNumberGetValue(num, kCFNumberSInt32Type, &out) != 0;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return CFNumberGetValue(num, kCFNumberSInt32Type, &out) != 0;
    } else if constexpr (std::is_same_v<T, double>) {
        return CFNumberGetValue(num, kCFNumberFloat64Type, &out) != 0;
    }
    return false;
}

// Helper: read a CFString as a std::string
static bool CfStringGetString(CFStringRef str, std::string& out) {
    if (!str) return false;
    CFIndex len = CFStringGetLength(str);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    out.resize(maxSize);
    if (CFStringGetCString(str, &out[0], maxSize, kCFStringEncodingUTF8)) {
        out.resize(strlen(out.c_str()));
        return true;
    }
    out.clear();
    return false;
}

void PowerInfo::Detect() {
    batteries.clear();
    powerPlan.clear();
    isAcPlugged = false;
    systemPowerWatts = 0.0;

    // --- AC status via IOPowerSources ---
    CFTypeRef powerSourcesInfo = IOPSCopyPowerSourcesInfo();
    if (powerSourcesInfo) {
        CFStringRef powerSourceType = IOPSGetProvidingPowerSourceType(powerSourcesInfo);
        if (powerSourceType) {
            isAcPlugged = CFEqual(powerSourceType, CFSTR(kIOPSACPowerKey)) != 0;
        }
        CFRelease(powerSourcesInfo);
    }

    // --- Battery via AppleSmartBattery IOKit service ---
    io_service_t service = IOServiceGetMatchingService(
        kIOMasterPortDefault, IOServiceMatching("AppleSmartBattery"));
    if (service) {
        CFMutableDictionaryRef properties = nullptr;
        if (IORegistryEntryCreateCFProperties(service, &properties,
                                              kCFAllocatorDefault, 0) == KERN_SUCCESS && properties) {
            BatteryInfo bat;
            bat.name = "Internal Battery";
            bat.isAcConnected = isAcPlugged;

            // Manufacturer
            CFStringRef manufacturer = (CFStringRef)CFDictionaryGetValue(
                properties, CFSTR("Manufacturer"));
            if (manufacturer) {
                std::string mfr;
                if (CfStringGetString(manufacturer, mfr) && !mfr.empty()) {
                    bat.name = mfr + " Battery";
                }
            }

            // Device Name
            CFStringRef deviceName = (CFStringRef)CFDictionaryGetValue(
                properties, CFSTR("DeviceName"));
            if (deviceName) {
                std::string dname;
                if (CfStringGetString(deviceName, dname) && !dname.empty()) {
                    // Prepend manufacturer if available
                    std::string mfr;
                    if (manufacturer && CfStringGetString(manufacturer, mfr) && !mfr.empty()) {
                        bat.name = mfr + " " + dname;
                    } else {
                        bat.name = dname;
                    }
                }
            }

            // Design Capacity (mAh), Max Capacity (mAh), Current Capacity (mAh)
            int32_t designCapMAh = 0, maxCapMAh = 0, currentCapMAh = 0;
            int32_t voltageMV = 0;
            int32_t cycleCount = 0;
            bool isCharging = false;

            CFNumberRef num;

            num = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("DesignCapacity"));
            if (num) CfNumberGetValue(num, designCapMAh);

            num = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("MaxCapacity"));
            if (num) CfNumberGetValue(num, maxCapMAh);

            num = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("CurrentCapacity"));
            if (num) CfNumberGetValue(num, currentCapMAh);

            num = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("Voltage"));
            if (num) CfNumberGetValue(num, voltageMV);

            num = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("CycleCount"));
            if (num) CfNumberGetValue(num, cycleCount);

            // IsCharging is a CFBoolean
            CFBooleanRef chargingBool = (CFBooleanRef)CFDictionaryGetValue(
                properties, CFSTR("IsCharging"));
            if (chargingBool) {
                isCharging = CFBooleanGetValue(chargingBool) != 0;
            }

            // TimeRemaining (minutes)
            int32_t timeRemainingMin = 0;
            num = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("TimeRemaining"));
            if (num) CfNumberGetValue(num, timeRemainingMin);

            // Convert mAh to mWh using voltage
            // If voltage is 0 (unlikely but possible), use a nominal voltage of 11100 mV (11.1V)
            int32_t effectiveVoltage = (voltageMV > 0) ? voltageMV : 11100;

            bat.cycleCount = static_cast<uint32_t>(cycleCount);
            bat.voltage = static_cast<uint32_t>(effectiveVoltage);

            if (designCapMAh > 0) {
                bat.designCapacity = static_cast<uint64_t>(designCapMAh)
                                   * effectiveVoltage / 1000;
            }
            if (maxCapMAh > 0) {
                bat.fullChargeCapacity = static_cast<uint64_t>(maxCapMAh)
                                       * effectiveVoltage / 1000;
            }
            if (currentCapMAh > 0) {
                bat.currentCapacity = static_cast<uint64_t>(currentCapMAh)
                                    * effectiveVoltage / 1000;
            }

            // Charge percentage
            if (maxCapMAh > 0) {
                bat.chargePercent = 100.0 * static_cast<double>(currentCapMAh)
                                    / static_cast<double>(maxCapMAh);
                if (bat.chargePercent > 100.0) bat.chargePercent = 100.0;
            } else if (currentCapMAh > 0) {
                bat.chargePercent = 100.0; // unknown max but have current
            }

            // Wear level
            if (bat.designCapacity > 0) {
                bat.wearLevel = 100.0 * (1.0 - static_cast<double>(bat.fullChargeCapacity)
                                          / static_cast<double>(bat.designCapacity));
                if (bat.wearLevel < 0.0) bat.wearLevel = 0.0;
            }

            bat.isCharging = isCharging;

            // Time remaining: only meaningful when discharging
            if (!isAcPlugged && timeRemainingMin > 0) {
                bat.timeRemaining = static_cast<uint32_t>(timeRemainingMin);
            } else {
                bat.timeRemaining = 0;
            }

            batteries.push_back(std::move(bat));
            CFRelease(properties);
        }
        IOObjectRelease(service);
    }

    Logger::Info("PowerInfo: detection complete (" +
                 std::to_string(batteries.size()) + " batteries, AC=" +
                 std::to_string(isAcPlugged) + ")");
}

#else
#error "Unsupported platform"
#endif
