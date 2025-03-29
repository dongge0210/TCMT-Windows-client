#pragma once
#include <vector>
#include <string>
#include <d3d11.h>

#if defined(SUPPORT_NVIDIA_GPU)
#include <nvml.h>
#endif

#if defined(SUPPORT_DIRECTX)
#include <dxgi.h>
#endif
#include <wbemidl.h>

class WmiManager;

class GpuInfo {
public:
    struct GpuData {
        std::wstring name;
        std::wstring deviceId;
        uint64_t dedicatedMemory = 0;
        double coreClock = 0.0;
        bool isNvidia = false;
        bool isIntegrated = false;
        int computeCapabilityMajor = 0;
        int computeCapabilityMinor = 0;
        unsigned int temperature = 0;
    };

    GpuInfo(WmiManager& manager);
    ~GpuInfo();

    const std::vector<GpuData>& GetGpuData() const;

private:
    void DetectGpusViaWmi();
    void QueryIntelGpuInfo(int index);
    void QueryNvidiaGpuInfo(int index);

    WmiManager& wmiManager;
    IWbemServices* pSvc = nullptr;
    std::vector<GpuData> gpuList;
};