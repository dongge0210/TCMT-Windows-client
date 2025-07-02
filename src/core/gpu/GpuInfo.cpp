#include "GpuInfo.h"
#include "Logger.h"
#include "WmiManager.h"
#include <comutil.h>
#include <nvml.h>
#include <algorithm>  // Add this header for std::transform
#include <cctype>     // Add this header for character functions
#include <cwctype>    // Add this header for wide character functions like towlower

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

bool GpuInfo::IsVirtualGpu(const std::wstring& name) {
    // 扩展虚拟显卡检测列表
    const std::vector<std::wstring> virtualGpuNames = {
        L"Microsoft Basic Display Adapter",
        L"Microsoft Hyper-V Video",
        L"VMware SVGA 3D",
        L"VirtualBox Graphics Adapter",
        L"Todesk Virtual Display Adapter",
        L"Parsec Virtual Display Adapter",
        L"TeamViewer Display",
        L"AnyDesk Display",
        L"Remote Desktop Display",
        L"RDP Display",
        L"VNC Display",
        L"Citrix Display",
        L"Standard VGA Graphics Adapter",
        L"Generic PnP Monitor",
        L"Virtual Desktop Infrastructure",
        L"VDI Display",
        L"Cloud Display",
        L"Remote Graphics"
    };

    std::wstring lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), 
        [](wchar_t c) { return ::towlower(c); });

    for (const auto& virtualName : virtualGpuNames) {
        std::wstring lowerVirtualName = virtualName;
        std::transform(lowerVirtualName.begin(), lowerVirtualName.end(), lowerVirtualName.begin(), 
            [](wchar_t c) { return ::towlower(c); });
        
        if (lowerName.find(lowerVirtualName) != std::wstring::npos) {
            return true;
        }
    }

    // 检查关键词
    const std::vector<std::wstring> virtualKeywords = {
        L"virtual", L"remote", L"basic", L"generic", L"standard vga",
        L"rdp", L"vnc", L"citrix", L"vmware", L"virtualbox", L"hyper-v"
    };

    for (const auto& keyword : virtualKeywords) {
        if (lowerName.find(keyword) != std::wstring::npos) {
            return true;
        }
    }

    return false;
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

        // 改进的虚拟显卡检测
        data.isVirtual = IsVirtualGpu(data.name);
        
        // 记录所有GPU，包括虚拟GPU，但标记它们
        data.isNvidia = (data.name.find(L"NVIDIA") != std::wstring::npos);
        data.isIntegrated = (data.deviceId.find(L"VEN_8086") != std::wstring::npos);
        
        gpuList.push_back(data);
        
        // 记录GPU信息到日志
        std::string gpuNameStr(data.name.begin(), data.name.end());
        Logger::Info("检测到GPU: " + gpuNameStr + 
                    " (虚拟: " + (data.isVirtual ? "是" : "否") + 
                    ", NVIDIA: " + (data.isNvidia ? "是" : "否") + 
                    ", 集成: " + (data.isIntegrated ? "是" : "否") + ")");

        VariantClear(&vtName);
        VariantClear(&vtPnpId);
        VariantClear(&vtAdapterRAM);
        VariantClear(&vtCurrentClockSpeed);
        pclsObj->Release();
    }

    pEnumerator->Release();

    // 为NVIDIA GPU查询详细信息
    for (size_t i = 0; i < gpuList.size(); ++i) {
        if (gpuList[i].isNvidia && !gpuList[i].isVirtual) {
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
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(0, &device);
    if (NVML_SUCCESS != result) {
        Logger::Error("获取设备句柄失败: " + std::string(nvmlErrorString(result)));
        nvmlShutdown();
        return;
    }

    // 获取显存信息
    nvmlMemory_t memory;
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (NVML_SUCCESS == result) {
        gpuList[index].dedicatedMemory = memory.total;
    }

    // 获取核心频率，保持 MHz 单位
    unsigned int clockMHz = 0;
    result = nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &clockMHz);
    if (NVML_SUCCESS == result) {
        gpuList[index].coreClock = static_cast<double>(clockMHz); // 直接使用 MHz
    }

    // 获取计算能力
    int major, minor;
    result = nvmlDeviceGetCudaComputeCapability(device, &major, &minor);
    if (NVML_SUCCESS == result) {
        gpuList[index].computeCapabilityMajor = major;
        gpuList[index].computeCapabilityMinor = minor;
    }

    nvmlShutdown();
}

const std::vector<GpuInfo::GpuData>& GpuInfo::GetGpuData() const {
    return gpuList;
}