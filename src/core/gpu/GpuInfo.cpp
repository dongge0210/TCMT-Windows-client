#include "GpuInfo.h"
#include "Logger.h"
#include "WmiManager.h"
#include <comutil.h>
#include <nvml.h>

GpuInfo::GpuInfo(WmiManager& manager) : wmiManager(manager) {
    if (!wmiManager.IsInitialized()) {
        Logger::Error("WMI服务未初始化");
        return;
    }
    pSvc = wmiManager.GetWmiService();
    DetectGpusViaWmi();
}

GpuInfo::~GpuInfo() {
    Logger::Info("GPU信息检测结束");
}

void GpuInfo::DetectGpusViaWmi() {
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_VideoController"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );

    if (FAILED(hres)) {
        Logger::Error("WMI查询失败");
        return;
    }

    ULONG uReturn = 0;
    IWbemClassObject* pclsObj = nullptr;
    while (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK) {
        GpuData data;
        VARIANT vtName, vtPnpId, vtAdapterRAM, vtCurrentClockSpeed;
        VariantInit(&vtName);
        VariantInit(&vtPnpId);
        VariantInit(&vtAdapterRAM);
        VariantInit(&vtCurrentClockSpeed);

        if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtName, 0, 0)) && vtName.vt == VT_BSTR) {
            data.name = vtName.bstrVal;
        }

        if (SUCCEEDED(pclsObj->Get(L"PNPDeviceID", 0, &vtPnpId, 0, 0)) && vtPnpId.vt == VT_BSTR) {
            data.deviceId = vtPnpId.bstrVal;
        }

        if (SUCCEEDED(pclsObj->Get(L"AdapterRAM", 0, &vtAdapterRAM, 0, 0)) && vtAdapterRAM.vt == VT_UI4) {
            data.dedicatedMemory = static_cast<uint64_t>(vtAdapterRAM.uintVal);
        }

        if (SUCCEEDED(pclsObj->Get(L"CurrentClockSpeed", 0, &vtCurrentClockSpeed, 0, 0)) && vtCurrentClockSpeed.vt == VT_UI4) {
            data.coreClock = static_cast<double>(vtCurrentClockSpeed.uintVal) / 1e6;
        }

        bool isVirtual = (
            data.name.find(L"Todesk Virtual Display Adapter") != std::wstring::npos ||
            data.name.find(L"Microsoft Basic Display Adapter") != std::wstring::npos
        );

        if (!isVirtual) {
            data.isNvidia = (data.name.find(L"NVIDIA") != std::wstring::npos);
            data.isIntegrated = (data.deviceId.find(L"VEN_8086") != std::wstring::npos);
            gpuList.push_back(data);
        }

        VariantClear(&vtName);
        VariantClear(&vtPnpId);
        VariantClear(&vtAdapterRAM);
        VariantClear(&vtCurrentClockSpeed);
        pclsObj->Release();
    }

    pEnumerator->Release();

    for (size_t i = 0; i < gpuList.size(); ++i) {
        if (gpuList[i].isNvidia) {
            QueryNvidiaGpuInfo(static_cast<int>(i));
        }
    }
}

void GpuInfo::QueryIntelGpuInfo(int index) {
    IDXGIFactory* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        Logger::Error("无法创建DXGI工厂");
        return;
    }

    IDXGIAdapter* pAdapter = nullptr;
    if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter))) {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
            gpuList[index].dedicatedMemory = desc.DedicatedVideoMemory;
        }
        pAdapter->Release();
    }
    pFactory->Release();
}

void GpuInfo::QueryNvidiaGpuInfo(int index) {
    nvmlReturn_t initResult = nvmlInit();
    if (NVML_SUCCESS != initResult) {
        Logger::Error("NVML初始化失败: " + std::string(nvmlErrorString(initResult)));
        return;
    }

    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(0, &device); // 假设只有一个NVIDIA GPU
    if (NVML_SUCCESS != result) {
        Logger::Error("获取设备句柄失败: " + std::string(nvmlErrorString(result)));
        nvmlShutdown();
        return;
    }

    int major, minor;
    result = nvmlDeviceGetCudaComputeCapability(device, &major, &minor);
    if (NVML_SUCCESS != result) {
        Logger::Error("获取计算能力失败: " + std::string(nvmlErrorString(result)));
        nvmlShutdown();
        return;
    }

    gpuList[index].computeCapabilityMajor = major;
    gpuList[index].computeCapabilityMinor = minor;

    nvmlShutdown(); // 关闭NVML
}

const std::vector<GpuInfo::GpuData>& GpuInfo::GetGpuData() const {
    return gpuList;
}