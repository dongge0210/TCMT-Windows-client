// TpmBridge.h
#pragma once
#include <windows.h>
#include <vector>
#include "../DataStruct/DataStruct.h"

// TPM 状态代码
enum TpmStatus : uint8_t {
    StatusUnknown = 0,
    StatusOk = 1,
    StatusError = 2,
    StatusDisabled = 3
};

// TPM 供应商 ID (实际存储为 uint32_t)
enum TpmVendorId : uint32_t {
    VendorUnknown = 0,
    VendorAMD = 0x414D44,      // "AMD"
    VendorIntel = 0x494E5445,  // "INTE"
    VendorMicrosoft = 0x4D5343, // "MSC"
    VendorInfineon = 0x494648, // "IFX"
    VendorSTMicro = 0x53544D,  // "STM"
    VendorNuvoton = 0x4E5554,  // "NUT"
    VendorBroadcom = 0x425243, // "BRC"
    VendorNationalSemiconductor = 0x4E5343 // "NSC"
};

class TpmBridge {
public:
    // 初始化 TPM 桥接
    static bool Initialize();
    
    // 清理 TPM 桥接
    static void Cleanup();
    
    // 获取 TPM 信息
    static bool GetTpmInfo(TpmInfo& info);
    
    // 检查 TPM 是否存在
    static bool IsTpmPresent();
    
private:
    static bool initialized;
    static bool tpmPresent;
    
    // 内部方法
    static HRESULT GetTpmProperty(DWORD propertyId, LPBYTE buffer, DWORD bufferSize);
    static std::wstring GetVendorString(uint32_t vendorId);
    static TpmStatus CheckTpmStatus();
};