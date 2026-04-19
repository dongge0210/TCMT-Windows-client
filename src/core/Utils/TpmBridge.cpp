// TpmBridge.cpp
#include "TpmBridge.h"
#include "Logger.h"
#include <winternl.h>
#include <tbs.h>

// 静态成员初始化
bool TpmBridge::initialized = false;
bool TpmBridge::tpmPresent = false;

bool TpmBridge::Initialize() {
    if (initialized) return true;
    
    // 检查 TPM 是否存在
    tpmPresent = IsTpmPresent();
    
    if (tpmPresent) {
        Logger::Info("TPM 2.0 detected");
    } else {
        Logger::Info("No TPM 2.0 detected");
    }
    
    initialized = true;
    return true;
}

void TpmBridge::Cleanup() {
    initialized = false;
    tpmPresent = false;
}

bool TpmBridge::IsTpmPresent() {
    // 使用 TBS API 检查 TPM 是否存在
    TPM_DEVICE_INFO info = {};
    UINT32 size = sizeof(info);
    
    TBS_RESULT result = Tbsi_GetDeviceInfo(size, &info);
    
    if (result == TBS_SUCCESS) {
        // tpmVersion: 1 = 1.2, 2 = 2.0
        return (info.tpmVersion >= 1 && info.tpmVersion <= 2);
    }
    
    return false;
}

bool TpmBridge::GetTpmInfo(TpmInfo& info) {
    // 初始化返回结构
    memset(&info, 0, sizeof(TpmInfo));
    
    if (!initialized) {
        Initialize();
    }
    
    if (!tpmPresent) {
        info.isPresent = false;
        info.status = static_cast<uint8_t>(StatusDisabled);
        return true; // 返回成功但 TPM 不存在
    }
    
    info.isPresent = true;
    
    // 使用 TBS 获取 TPM 信息
    TBS_HCONTEXT hContext = NULL;
    
    // 创建 TBS 上下文 (使用版本 2 支持 TPM 2.0)
    TBS_CONTEXT_PARAMS2 params2 = {};
    params2.version = TBS_CONTEXT_VERSION_TWO;
    params2.includeTpm20 = 1;  // 请求 TPM 2.0 支持
    
    // 使用 PCTBS_CONTEXT_PARAMS 强制转换
    PCTBS_CONTEXT_PARAMS pParams = (PCTBS_CONTEXT_PARAMS)&params2;
    TBS_RESULT result = Tbsi_Context_Create(pParams, &hContext);
    
    if (result != TBS_SUCCESS) {
        Logger::Warn("Failed to create TBS context");
        info.status = static_cast<uint8_t>(StatusError);
        return false;
    }
    
    // 获取 TPM 设备信息
    TPM_DEVICE_INFO tpmInfo = {};
    UINT32 infoSize = sizeof(tpmInfo);
    result = Tbsi_GetDeviceInfo(infoSize, &tpmInfo);
    
    if (result == TBS_SUCCESS) {
        // TPM 版本
        if (tpmInfo.tpmVersion == 1) {
            swprintf_s(info.firmwareVersion, L"1.2");
            info.firmwareVersionMajor = 1;
            info.firmwareVersionMinor = 2;
        } else if (tpmInfo.tpmVersion == 2) {
            swprintf_s(info.firmwareVersion, L"2.0");
            info.firmwareVersionMajor = 2;
            info.firmwareVersionMinor = 0;
        }
        
        // 接口类型
        switch (tpmInfo.tpmInterfaceType) {
            case TPM_IFTYPE_HW:
                wcsncpy_s(info.manufacturer, L"Hardware TPM", _TRUNCATE);
                break;
            case TPM_IFTYPE_EMULATOR:
                wcsncpy_s(info.manufacturer, L"Software Emulator", _TRUNCATE);
                break;
            default:
                wcsncpy_s(info.manufacturer, L"Unknown", _TRUNCATE);
                break;
        }
        
        // 供应商 ID 从实现修订版获取
        info.vendorId = static_cast<uint16_t>(tpmInfo.tpmImpRevision & 0xFFFF);
    }
    
    // 清理上下文
    if (hContext) {
        Tbsip_Context_Close(hContext);
    }
    
    // 检查 TPM 状态
    info.status = static_cast<uint8_t>(CheckTpmStatus());
    
    // 设置默认启用和激活状态
    info.isEnabled = (info.status == static_cast<uint8_t>(StatusOk) || info.status == static_cast<uint8_t>(StatusUnknown));
    info.isActive = (info.status == static_cast<uint8_t>(StatusOk));
    
    // 自检状态 - 简化处理
    info.selfTestStatus = (info.status == static_cast<uint8_t>(StatusOk)) ? 0 : 1;
    
    // 输出 TPM 详细信息用于调试
    Logger::Info("TPM 调试: isPresent=" + std::to_string(info.isPresent) + 
                 ", status=" + std::to_string(static_cast<int>(info.status)) +
                 ", isEnabled=" + std::to_string(info.isEnabled) +
                 ", isActive=" + std::to_string(info.isActive));
    
    return true;
}

std::wstring TpmBridge::GetVendorString(uint32_t vendorId) {
    // 从 vendor ID 提取可读字符串 (低 3 字节)
    char vendorStr[5] = {};
    vendorStr[0] = static_cast<char>((vendorId >> 16) & 0xFF);
    vendorStr[1] = static_cast<char>((vendorId >> 8) & 0xFF);
    vendorStr[2] = static_cast<char>(vendorId & 0xFF);
    vendorStr[3] = '\0';
    
    // 转换为常见厂商名称
    if (strcmp(vendorStr, "AMD") == 0) return L"AMD";
    if (strcmp(vendorStr, "INTE") == 0) return L"Intel";
    if (strcmp(vendorStr, "MSC") == 0) return L"Microsoft";
    if (strcmp(vendorStr, "IFX") == 0) return L"Infineon";
    if (strcmp(vendorStr, "STM") == 0) return L"STMicroelectronics";
    if (strcmp(vendorStr, "NUT") == 0) return L"Nuvoton";
    if (strcmp(vendorStr, "BRC") == 0) return L"Broadcom";
    if (strcmp(vendorStr, "NSC") == 0) return L"National Semiconductor";
    
    // 尝试直接转换为宽字符
    wchar_t result[32] = {};
    mbstowcs_s(nullptr, result, vendorStr, _TRUNCATE);
    return result;
}

TpmStatus TpmBridge::CheckTpmStatus() {
    // 使用 TBS 获取 TPM 设备状态
    TPM_DEVICE_INFO info = {};
    UINT32 size = sizeof(info);
    
    TBS_RESULT result = Tbsi_GetDeviceInfo(size, &info);
    
    if (result != TBS_SUCCESS) {
        return StatusError;
    }
    
    switch (info.tpmVersion) {
        case 1:
        case 2:
            return StatusOk;
        default:
            return StatusDisabled;
    }
}