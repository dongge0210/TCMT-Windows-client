// file: src/core/tpm/TpmInfo.cpp
#include "TpmInfo.h"
#include "../Utils/Logger.h"

#ifdef PLATFORM_WINDOWS
    #include "../Utils/WmiManager.h"
    #include "../Utils/WinUtils.h"
    #define WIN32_LEAN_AND_MEAN
    #include <tbs.h>
    #include <wbemidl.h>
    #include <comutil.h>
    #include <string>
    #include <cwchar>
    #pragma comment(lib, "tbs.lib")
    #pragma comment(lib, "ole32.lib")
    #pragma comment(lib, "oleaut32.lib")
    #pragma comment(lib, "wbemuuid.lib")
#elif defined(PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <IOKit/IOKitLib.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <unistd.h>
    #include <fstream>
    #include <sstream>
#elif defined(PLATFORM_LINUX)
    #include <sys/sysinfo.h>
    #include <fstream>
    #include <sstream>
    #include <unistd.h>
    #include <glob.h>
#endif

namespace {
    struct TbsContextGuard {
        TBS_HCONTEXT ctx{ 0 };
        ~TbsContextGuard() {
            if (ctx) {
                Tbsip_Context_Close(ctx);
                ctx = 0;
            }
        }
    };

    std::wstring MapTbsError(TBS_RESULT r) {
        switch (r) {
        case TBS_E_TPM_NOT_FOUND:        return L"TPM not found";
        case TBS_E_SERVICE_NOT_RUNNING:  return L"TBS service not running";
        case TBS_E_INSUFFICIENT_BUFFER:  return L"Insufficient buffer";
#ifdef TBS_E_BAD_PARAMETER
        case TBS_E_BAD_PARAMETER:        return L"Bad parameter";
#endif
#ifdef TBS_E_INTERNAL_ERROR
        case TBS_E_INTERNAL_ERROR:       return L"Internal error";
#endif
#ifdef TBS_E_IOERROR
        case TBS_E_IOERROR:              return L"I/O error";
#endif
#ifdef TBS_E_ACCESS_DENIED
        case TBS_E_ACCESS_DENIED:        return L"Access denied";
#endif
        default: {
            wchar_t buf[48];
            swprintf_s(buf, L"TBS error 0x%08X", r);
            return buf;
        }
        }
    }

    void SetStatusIfEmpty(std::wstring& status, const std::wstring& value) {
        if (status.empty() || status == L"Unknown") status = value;
    }
}

#ifdef PLATFORM_WINDOWS
TpmInfo::TpmInfo(WmiManager& manager) : wmiManager(&manager) {
    hasTpm = false;
    tpmData.detectionMethod = "NONE";
    tpmData.wmiDetectionWorked = false;
    tpmData.tbsDetectionWorked = false;
    tpmData.status = "Unknown";
    tpmData.errorMessage.clear();

    Logger::Info("TPM detection start (TBS primary, WMI fallback)");

    try { DetectTpmViaTbs(); }
    catch (...) { Logger::Warn("TBS detection threw an exception"); }
    try { DetectTpmViaWmi(); }
    catch (...) { Logger::Warn("WMI detection threw an exception"); }

    DetermineDetectionMethod();

    if (hasTpm) {
        Logger::Info("TPM detected: " + tpmData.manufacturerName + " v" + 
                     tpmData.version + " (" + tpmData.status + ") [method: " + 
                     tpmData.detectionMethod + "]");
    }
    else {
        Logger::Info("No TPM detected: " + tpmData.errorMessage);
    }
}
#else
TpmInfo::TpmInfo() {
    hasTpm = false;
    tpmData.detectionMethod = "NONE";
    tpmData.isSupported = false;
    tpmData.status = "Unknown";
    tpmData.errorMessage.clear();

    DetectTpm();

    if (hasTpm) {
        Logger::Info("TPM detected: " + tpmData.manufacturerName + " v" + 
                     tpmData.version + " (" + tpmData.status + ") [method: " + 
                     tpmData.detectionMethod + "]");
    }
    else {
        Logger::Info("TPM status: " + tpmData.platformInfo);
    }
}
#endif
}

TpmInfo::~TpmInfo() {
    Logger::Debug("TPM detection end");
}

void TpmInfo::DetectTpm() {
    try {
#ifdef PLATFORM_WINDOWS
        DetectTpmViaTbs();
        DetectTpmViaWmi();
        DetermineDetectionMethod();
#elif defined(PLATFORM_MACOS)
        DetectMacTpm();
#elif defined(PLATFORM_LINUX)
        DetectLinuxTpm();
#endif
    }
    catch (const std::exception& e) {
        Logger::Error("DetectTpm failed: " + std::string(e.what()));
        tpmData.errorMessage = "Detection failed: " + std::string(e.what());
    }
}

// macOS TPM检测实现
#ifdef PLATFORM_MACOS
void TpmInfo::DetectMacTpm() {
    Logger::Debug("DetectMacTpm: Starting macOS TPM detection");
    
    tpmData.detectionMethod = "macOS System Integrity";
    tpmData.isSupported = CheckMacTpmSupport();
    
    if (!tpmData.isSupported) {
        tpmData.platformInfo = "macOS does not support TPM (uses Secure Enclave instead)";
        Logger::Info("TPM: macOS uses Secure Enclave instead of TPM");
        return;
    }
    
    // 获取系统完整性保护信息
    std::string sipStatus = GetMacSystemIntegrityProtection();
    tpmData.platformInfo = "macOS Secure Enclave Status: " + sipStatus;
    tpmData.status = sipStatus;
    
    // macOS使用Secure Enclave而不是TPM
    tpmData.manufacturerName = "Apple";
    tpmData.version = "Secure Enclave";
    tpmData.isEnabled = true;
    tpmData.isActivated = true;
    tpmData.isReady = true;
    tpmData.isOwned = true;
    tpmData.specVersion = 2; // Secure Enclave 2.0+
    
    hasTpm = true;
    Logger::Info("TPM: macOS Secure Enclave detected - " + sipStatus);
}

bool TpmInfo::CheckMacTpmSupport() {
    try {
        // 检查是否有Secure Enclave
        FILE* pipe = popen("system_profiler SPiBridgeDataType | grep -i \"secure enclave\"", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                std::string line(buffer);
                if (line.find("Secure Enclave") != std::string::npos) {
                    pclose(pipe);
                    return true;
                }
            }
            pclose(pipe);
        }
        
        // 检查系统完整性保护
        FILE* pipe2 = popen("csrutil status | grep \"System Integrity Protection\"", "r");
        if (pipe2) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe2)) {
                std::string line(buffer);
                if (line.find("System Integrity Protection status") != std::string::npos) {
                    pclose(pipe2);
                    return true;
                }
            }
            pclose(pipe2);
        }
    }
    catch (const std::exception& e) {
        Logger::Error("CheckMacTpmSupport failed: " + std::string(e.what()));
    }
    
    return false;
}

std::string TpmInfo::GetMacSystemIntegrityProtection() {
    try {
        FILE* pipe = popen("csrutil status", "r");
        if (!pipe) return "Unknown";
        
        char buffer[1024];
        std::string result;
        
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        
        // 解析SIP状态
        if (result.find("System Integrity Protection status: enabled") != std::string::npos) {
            return "Enabled";
        } else if (result.find("System Integrity Protection status: disabled") != std::string::npos) {
            return "Disabled";
        } else if (result.find("System Integrity Protection status: unknown") != std::string::npos) {
            return "Unknown";
        }
        
        return "Not Available";
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSystemIntegrityProtection failed: " + std::string(e.what()));
        return "Error";
    }
}
#endif

// Linux TPM检测实现
#ifdef PLATFORM_LINUX
void TpmInfo::DetectLinuxTpm() {
    Logger::Debug("DetectLinuxTpm: Starting Linux TPM detection");
    
    tpmData.detectionMethod = "Linux Device Check";
    
    if (!CheckLinuxTpmDevice()) {
        tpmData.platformInfo = "No TPM device found on this system";
        Logger::Info("TPM: No TPM device found on this Linux system");
        return;
    }
    
    // 获取TPM版本信息
    std::string version = GetLinuxTpmVersion();
    tpmData.version = version;
    
    // 检查TPM设备状态
    FILE* pipe = popen("tpm2_getcap -c 2>/dev/null | head -20", "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            if (line.find("TPM") != std::string::npos) {
                tpmData.status = line;
                break;
            }
        }
        pclose(pipe);
    }
    
    // 设置基本状态
    tpmData.isEnabled = true;
    tpmData.isActivated = true;
    tpmData.isReady = true;
    tpmData.isOwned = true;
    tpmData.manufacturerName = "Unknown";
    
    if (version.find("2.0") != std::string::npos) {
        tpmData.specVersion = 2;
    } else if (version.find("1.2") != std::string::npos) {
        tpmData.specVersion = 1;
    } else {
        tpmData.specVersion = 0;
    }
    
    hasTpm = true;
    Logger::Info("TPM: Linux TPM detected - " + version);
}

bool TpmInfo::CheckLinuxTpmDevice() {
    // 检查常见的TPM设备路径
    const std::vector<std::string> tpmDevices = {
        "/dev/tpm0", "/dev/tpmrm0", "/dev/tpm1", "/dev/tpmrm1"
    };
    
    for (const auto& device : tpmDevices) {
        if (access(device.c_str(), F_OK) == 0) {
            Logger::Debug("Found TPM device: " + device);
            return true;
        }
    }
    
    // 检查sysfs中的TPM
    if (access("/sys/class/tpm/tpm0", F_OK) == 0) {
        Logger::Debug("Found TPM sysfs device");
        return true;
    }
    
    return false;
}

std::string TpmInfo::GetLinuxTpmVersion() {
    FILE* pipe = popen("tpm2_getcap -v 2>/dev/null | head -5", "r");
    if (!pipe) return "Unknown";
    
    char buffer[256];
    std::string result;
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    
    // 解析版本信息
    if (result.find("TPM 2.0") != std::string::npos) {
        return "2.0";
    } else if (result.find("TPM 1.2") != std::string::npos) {
        return "1.2";
    } else if (result.find("TPM 1.1") != std::string::npos) {
        return "1.1";
    }
    
    return "Unknown";
}
#endif

// Windows实现
void TpmInfo::DetectTpmViaWmi() {
    if (!wmiManager.IsInitialized()) {
        Logger::Warn("WMI manager not initialized");
        return;
    }

    IWbemServices* pSvc = wmiManager.GetWmiService();
    if (!pSvc) {
        Logger::Warn("WMI service not available");
        return;
    }

    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_Tpm"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );

    if (FAILED(hres) || !pEnumerator) {
        Logger::Debug("Win32_Tpm query failed or returned null");
        return;
    }

    ULONG uReturn = 0;
    IWbemClassObject* pclsObj = nullptr;
    bool found = false;

    while (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK) {
        found = true;

        VARIANT vtManufacturerName{}, vtManufacturerId{}, vtSpecVersion{};
        VARIANT vtIsEnabled{}, vtIsActivated{}, vtIsOwned{}, vtPhysicalPresenceRequired{};

        VariantInit(&vtManufacturerName);
        VariantInit(&vtManufacturerId);
        VariantInit(&vtSpecVersion);
        VariantInit(&vtIsEnabled);
        VariantInit(&vtIsActivated);
        VariantInit(&vtIsOwned);
        VariantInit(&vtPhysicalPresenceRequired);

        if (SUCCEEDED(pclsObj->Get(L"ManufacturerName", 0, &vtManufacturerName, 0, 0)) && vtManufacturerName.vt == VT_BSTR) {
            // Convert BSTR to std::string
            int len = SysStringLen(vtManufacturerName.bstrVal);
            if (len > 0) {
                std::wstring wstr(vtManufacturerName.bstrVal, len);
                tpmData.manufacturerName = std::string(wstr.begin(), wstr.end());
            }
        }

        if (SUCCEEDED(pclsObj->Get(L"ManufacturerId", 0, &vtManufacturerId, 0, 0)) && vtManufacturerId.vt == VT_I4) {
            wchar_t manufacturerIdStr[32];
            swprintf_s(manufacturerIdStr, L"0x%08X", vtManufacturerId.intVal);
            tpmData.manufacturerId = manufacturerIdStr;
        }

        if (SUCCEEDED(pclsObj->Get(L"SpecVersion", 0, &vtSpecVersion, 0, 0)) && vtSpecVersion.vt == VT_BSTR) {
            // Convert BSTR to std::string
            int len = SysStringLen(vtSpecVersion.bstrVal);
            if (len > 0) {
                std::wstring wstr(vtSpecVersion.bstrVal, len);
                tpmData.version = std::string(wstr.begin(), wstr.end());
            }
        }

        if (SUCCEEDED(pclsObj->Get(L"IsEnabled_InitialValue", 0, &vtIsEnabled, 0, 0)) && vtIsEnabled.vt == VT_BOOL)
            tpmData.isEnabled = (vtIsEnabled.boolVal == VARIANT_TRUE);

        if (SUCCEEDED(pclsObj->Get(L"IsActivated_InitialValue", 0, &vtIsActivated, 0, 0)) && vtIsActivated.vt == VT_BOOL)
            tpmData.isActivated = (vtIsActivated.boolVal == VARIANT_TRUE);

        if (SUCCEEDED(pclsObj->Get(L"IsOwned_InitialValue", 0, &vtIsOwned, 0, 0)) && vtIsOwned.vt == VT_BOOL)
            tpmData.isOwned = (vtIsOwned.boolVal == VARIANT_TRUE);

        if (SUCCEEDED(pclsObj->Get(L"PhysicalPresenceRequired", 0, &vtPhysicalPresenceRequired, 0, 0)) && vtPhysicalPresenceRequired.vt == VT_BOOL)
            tpmData.physicalPresenceRequired = (vtPhysicalPresenceRequired.boolVal == VARIANT_TRUE);

        tpmData.isReady = tpmData.isEnabled && tpmData.isActivated;
        if (tpmData.isReady)          tpmData.status = "Ready";
        else if (tpmData.isEnabled && !tpmData.isActivated) tpmData.status = "EnabledNotActivated";
        else if (!tpmData.isEnabled)  tpmData.status = "Disabled";
        else                          tpmData.status = "Unknown";

        VariantClear(&vtManufacturerName);
        VariantClear(&vtManufacturerId);
        VariantClear(&vtSpecVersion);
        VariantClear(&vtIsEnabled);
        VariantClear(&vtIsActivated);
        VariantClear(&vtIsOwned);
        VariantClear(&vtPhysicalPresenceRequired);

        pclsObj->Release();
        break;
    }

    pEnumerator->Release();

    if (found) {
        tpmData.wmiDetectionWorked = true;
        hasTpm = true;
    }
}

void TpmInfo::DetectTpmViaTbs() {
    TbsContextGuard guard;
    TBS_CONTEXT_PARAMS params{};
    params.version = TBS_CONTEXT_VERSION_ONE;

    TBS_RESULT result = Tbsi_Context_Create(&params, &guard.ctx);
    if (result == TBS_SUCCESS) {
        tpmData.tbsAvailable = true;

        TPM_DEVICE_INFO deviceInfo{};
        result = Tbsi_GetDeviceInfo(sizeof(deviceInfo), &deviceInfo);
        if (result == TBS_SUCCESS) {
            tpmData.tbsVersion = deviceInfo.tpmVersion;
            if (deviceInfo.tpmVersion == TPM_VERSION_12) {
            if (tpmData.status.empty() || tpmData.status == "Unknown") tpmData.status = "DetectedViaTBS";
            if (tpmData.version.empty()) tpmData.version = "1.2";
        }
        else if (deviceInfo.tpmVersion == TPM_VERSION_20) {
            if (tpmData.status.empty() || tpmData.status == "Unknown") tpmData.status = "DetectedViaTBS";
            if (tpmData.version.empty()) tpmData.version = "2.0";
        }
        }

        hasTpm = true;
        tpmData.tbsDetectionWorked = true;
        if (tpmData.status.empty() || tpmData.status == "Unknown") tpmData.status = "DetectedViaTBS";
    }
    else {
        tpmData.tbsAvailable = false;
        tpmData.errorMessage = MapTbsError(result);
        // do not set hasTpm=true here
    }
}

const TpmInfo::TpmData& TpmInfo::GetTpmData() const {
    return tpmData;
}

void TpmInfo::DetermineDetectionMethod() {
    if (tpmData.tbsDetectionWorked && tpmData.wmiDetectionWorked) {
        tpmData.detectionMethod = "TBS+WMI";
    }
    else if (tpmData.tbsDetectionWorked) {
        tpmData.detectionMethod = "TBS";
    }
    else if (tpmData.wmiDetectionWorked) {
        tpmData.detectionMethod = "WMI";
    }
    else {
        tpmData.detectionMethod = "NONE";
    }
}