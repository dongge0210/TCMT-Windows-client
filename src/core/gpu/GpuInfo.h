#pragma once
#include <vector>
#include <string>

#ifdef PLATFORM_WINDOWS
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
    bool isVirtual = false;
    int computeCapabilityMajor = 0;
    int computeCapabilityMinor = 0;
    unsigned int temperature = 0;
    
    // 跨平台图形API支持状态
    bool supportsD3D12 = false;
    std::wstring d3d12Version;
    bool supportsMetal = false;
    std::wstring metalVersion;
    bool supportsOpenGL = false;
    std::wstring openGLVersion;
    bool supportsVulkan = false;
    std::wstring vulkanVersion;
    std::wstring vulkanDriverVersion;
};

        GpuInfo(WmiManager& manager);
        ~GpuInfo();

        const std::vector<GpuData>& GetGpuData() const;

    private:
    void QueryGpuInfo();
    bool IsVirtualGpu(const std::wstring& name);
    
    // WMI服务相关
    WmiManager& wmiManager;
    IWbemServices* pSvc = nullptr;
    
    // GPU检测方法
    void DetectGpusViaWmi();
    bool IsNvidiaGpu(const std::wstring& name);
    bool IsIntegratedGpu(const std::wstring& name);
#ifdef CUDA_SUPPORTED
    void QueryNvidiaGpuDetails(GpuData& gpu);
#endif
    
    // 跨平台图形API检测方法
    void DetectGraphicsAPIs(GpuData& gpu);
    bool DetectD3D12Support(GpuData& gpu);
    bool DetectMetalSupport(GpuData& gpu);
    bool DetectOpenGLSupport(GpuData& gpu);
    bool DetectVulkanSupport(GpuData& gpu);
    
    std::vector<GpuData> m_gpuData;
    };
#else
    // Cross-platform stub implementation
    class GpuInfo {
    public:
        struct GpuData {
            std::wstring name;
            std::wstring deviceId;
            uint64_t dedicatedMemory = 0;
            double coreClock = 0.0;
            bool isNvidia = false;
            bool isIntegrated = false;
            bool isVirtual = false;
            int computeCapabilityMajor = 0;
            int computeCapabilityMinor = 0;
            unsigned int temperature = 0;
        };

        GpuInfo() = default;
        ~GpuInfo() = default;
        
        const std::vector<GpuData>& GetGpuData() const {
            return gpuList;
        }

    private:
        std::vector<GpuData> gpuList;
    };
#endif