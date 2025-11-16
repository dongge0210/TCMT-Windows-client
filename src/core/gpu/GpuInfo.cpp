#include "GpuInfo.h"
#include "../Utils/Logger.h"
#include "../Utils/CrossPlatformSystemInfo.h"

#ifdef PLATFORM_WINDOWS
    #include "WmiManager.h"
    #include <comutil.h>
    #include <d3d12.h>
    #include <dxgi1_6.h>
    #include <wrl/client.h> // For Microsoft::WRL::ComPtr
    // CUDA 支持 - 仅在 CUDA_SUPPORTED 定义时包含
    #ifdef CUDA_SUPPORTED
        #include <nvml.h>
    #endif
    // Vulkan 支持
    #define VK_USE_PLATFORM_WIN32_KHR
    #include <vulkan/vulkan.h>
    // 动态加载Vulkan
    #include <windows.h>
#endif

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
        L"Remote Graphics",
        L"AskLinkIddDriver Device"
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

void GpuInfo::QueryGpuInfo() {
    try {
        m_gpuData.clear();
        
#ifdef PLATFORM_WINDOWS
        // 使用 WMI 查询 GPU 信息
        std::wstring query = L"SELECT * FROM Win32_VideoController";
        auto results = wmiManager.ExecuteQuery(query);
        
        for (const auto& result : results) {
            GpuData gpu;
            
            // 基本信息
            auto name = result[L"Name"];
            auto description = result[L"Description"];
            auto adapterRam = result[L"AdapterRAM"];
            auto driverVersion = result[L"DriverVersion"];
            auto videoProcessor = result[L"VideoProcessor"];
            auto availability = result[L"Availability"];
            
            if (!name.empty()) {
                gpu.name = name;
            }
            
            if (!description.empty()) {
                gpu.deviceId = description;
            }
            
            if (!adapterRam.empty()) {
                try {
                    gpu.dedicatedMemory = std::stoull(adapterRam);
                }
                catch (...) {
                    gpu.dedicatedMemory = 0;
                }
            }
            
            // 检测是否为 NVIDIA GPU
            gpu.isNvidia = IsNvidiaGpu(gpu.name);
            
            // 检测是否为集成显卡
            gpu.isIntegrated = IsIntegratedGpu(gpu.name);
            
            // 检测是否为虚拟 GPU
            gpu.isVirtual = IsVirtualGpu(gpu.name);
            
            // 查询 NVIDIA GPU 的详细信息
            #ifdef CUDA_SUPPORTED
            if (gpu.isNvidia) {
                QueryNvidiaGpuDetails(gpu);
            }
            #endif

            // 检测图形API支持
            DetectGraphicsAPIs(gpu);

            m_gpuData.push_back(gpu);
        }
#else
        // 使用跨平台系统信息管理器
        auto& systemInfo = CrossPlatformSystemInfo::GetInstance();
        if (systemInfo.Initialize()) {
            auto gpuDevices = systemInfo.GetGpuDevices();
            
            for (const auto& device : gpuDevices) {
                GpuData gpu;
                
                // 转换设备信息到 GPU 数据结构
                gpu.name = std::wstring(device.name.begin(), device.name.end());
                gpu.deviceId = std::wstring(device.deviceId.begin(), device.deviceId.end());
                
                // 从属性中提取内存信息
                auto memoryIt = device.properties.find("memory");
                if (memoryIt != device.properties.end()) {
                    try {
                        gpu.dedicatedMemory = std::stoull(memoryIt->second);
                    }
                    catch (...) {
                        gpu.dedicatedMemory = 0;
                    }
                }
                
                // 检测 GPU 类型
                gpu.isNvidia = IsNvidiaGpu(gpu.name);
                gpu.isIntegrated = IsIntegratedGpu(gpu.name);
                gpu.isVirtual = IsVirtualGpu(gpu.name);
                
                // 获取驱动版本信息
                auto driverIt = device.properties.find("driver_version");
                if (driverIt != device.properties.end()) {
                    // 可以设置额外的驱动版本信息
                }
                
                // 检测图形API支持
                DetectGraphicsAPIs(gpu);
                
                m_gpuData.push_back(gpu);
            }
        }
        
        // 如果没有找到 GPU，添加一个默认条目
        if (m_gpuData.empty()) {
            GpuData defaultGpu;
            defaultGpu.name = L"Unknown GPU";
            defaultGpu.isVirtual = systemInfo.IsVirtualMachine();
            m_gpuData.push_back(defaultGpu);
        }
#endif
    }
    catch (const std::exception& e) {
        Logger::Error("GpuInfo: Exception in QueryGpuInfo: " + std::string(e.what()));
    }
}

void GpuInfo::QueryIntelGpuInfo(int index) {
#if defined(SUPPORT_DIRECTX)
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
#else
    Logger::Info("DirectX 未启用，跳过 Intel GPU 信息查询");
#endif
}

void GpuInfo::QueryNvidiaGpuInfo(int index) {
#ifdef CUDA_SUPPORTED
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
        gpuList[index].coreClock = static_cast<double>(clockMHz);
    }

    // 获取温度
    unsigned int temp = 0;
    nvmlReturn_t tempResult = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);
    if (NVML_SUCCESS == tempResult) {
        gpuList[index].temperature = temp;
    }

    // 获取计算能力
    int major, minor;
    result = nvmlDeviceGetCudaComputeCapability(device, &major, &minor);
    if (NVML_SUCCESS == result) {
        gpuList[index].computeCapabilityMajor = major;
        gpuList[index].computeCapabilityMinor = minor;
    }

    nvmlShutdown();
#else
    Logger::Info("CUDA 未启用，跳过 NVIDIA GPU 信息查询");
#endif
}

const std::vector<GpuInfo::GpuData>& GpuInfo::GetGpuData() const {
    return gpuList;
}// 跨平台图形API检测实现
void GpuInfo::DetectGraphicsAPIs(GpuData& gpu) {
#ifdef PLATFORM_WINDOWS
    DetectD3D12Support(gpu);
    DetectOpenGLSupport(gpu);
    DetectVulkanSupport(gpu);
#elif defined(__APPLE__)
    DetectMetalSupport(gpu);
    DetectOpenGLSupport(gpu);
    DetectVulkanSupport(gpu);
#else
    DetectOpenGLSupport(gpu);
    DetectVulkanSupport(gpu);
#endif
}

bool GpuInfo::DetectD3D12Support(GpuData& gpu) {
#ifdef PLATFORM_WINDOWS
    try {
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D12Device> device;
        hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) {
            gpu.supportsD3D12 = true;
            gpu.d3d12Version = L"12.0";
            
            D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = {};
            D3D_FEATURE_LEVEL levels[] = {
                D3D_FEATURE_LEVEL_12_2,
                D3D_FEATURE_LEVEL_12_1,
                D3D_FEATURE_LEVEL_12_0
            };
            featureLevels.NumFeatureLevels = ARRAYSIZE(levels);
            featureLevels.pFeatureLevelsRequested = levels;
            
            hr = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
            if (SUCCEEDED(hr) && featureLevels.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_12_1) {
                gpu.d3d12Version = L"12.1";
            }
            if (SUCCEEDED(hr) && featureLevels.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_12_2) {
                gpu.d3d12Version = L"12.2";
            }
            
            return true;
        }
    }
    catch (...) {
    }
#endif
    return false;
}

bool GpuInfo::DetectMetalSupport(GpuData& gpu) {
#if defined(__APPLE__)
    @try {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            gpu.supportsMetal = true;
            
            NSOperatingSystemVersion osVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
            if (osVersion.majorVersion >= 13) {
                gpu.metalVersion = L"3.0";
            } else if (osVersion.majorVersion >= 12) {
                gpu.metalVersion = L"2.4";
            } else if (osVersion.majorVersion >= 11) {
                gpu.metalVersion = L"2.0";
            } else if (osVersion.majorVersion >= 10) {
                gpu.metalVersion = L"1.2";
            } else {
                gpu.metalVersion = L"1.0";
            }
            
            [device release];
            return true;
        }
    }
    @catch (...) {
    }
#endif
    return false;
}

bool GpuInfo::DetectOpenGLSupport(GpuData& gpu) {
    try {
#ifdef PLATFORM_WINDOWS
        HWND hWnd = GetDesktopWindow();
        HDC hDC = GetDC(hWnd);
        
        PIXELFORMATDESCRIPTOR pfd = {0};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 24;
        
        int pixelFormat = ChoosePixelFormat(hDC, &pfd);
        if (pixelFormat != 0) {
            SetPixelFormat(hDC, pixelFormat, &pfd);
            
            HGLRC hGLRC = wglCreateContext(hDC);
            if (hGLRC) {
                wglMakeCurrent(hDC, hGLRC);
                
                const char* version = (const char*)glGetString(GL_VERSION);
                if (version) {
                    gpu.supportsOpenGL = true;
                    std::string versionStr(version);
                    gpu.openGLVersion = std::wstring(versionStr.begin(), versionStr.end());
                }
                
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(hGLRC);
            }
        }
        
        ReleaseDC(hWnd, hDC);
#elif defined(__APPLE__)
        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAccelerated,
            0
        };
        
        NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        if (pixelFormat) {
            NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
            if (context) {
                [context makeCurrentContext];
                
                const char* version = (const char*)glGetString(GL_VERSION);
                if (version) {
                    gpu.supportsOpenGL = true;
                    std::string versionStr(version);
                    gpu.openGLVersion = std::wstring(versionStr.begin(), versionStr.end());
                }
                
                [NSOpenGLContext clearCurrentContext];
                [context release];
            }
            [pixelFormat release];
        }
#else
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display != EGL_NO_DISPLAY) {
            EGLint major, minor;
            if (eglInitialize(display, &major, &minor)) {
                EGLConfig config;
                EGLint numConfigs;
                EGLint configAttribs[] = {
                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                    EGL_RED_SIZE, 8,
                    EGL_GREEN_SIZE, 8,
                    EGL_BLUE_SIZE, 8,
                    EGL_ALPHA_SIZE, 8,
                    EGL_DEPTH_SIZE, 24,
                    EGL_NONE
                };
                
                if (eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
                    EGLSurface surface = eglCreateWindowSurface(display, config, nullptr, nullptr);
                    if (surface != EGL_NO_SURFACE) {
                        EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
                        if (context != EGL_NO_CONTEXT) {
                            if (eglMakeCurrent(display, surface, surface, context)) {
                                const char* version = (const char*)glGetString(GL_VERSION);
                                if (version) {
                                    gpu.supportsOpenGL = true;
                                    std::string versionStr(version);
                                    gpu.openGLVersion = std::wstring(versionStr.begin(), versionStr.end());
                                }
                            }
                            eglDestroyContext(display, context);
                        }
                        eglDestroySurface(display, surface);
                    }
                }
                eglTerminate(display);
            }
        }
#endif
    }
    catch (...) {
    }
    return gpu.supportsOpenGL;
}

bool GpuInfo::DetectVulkanSupport(GpuData& gpu) {
    try {
#ifdef PLATFORM_WINDOWS
        HMODULE vulkanLib = LoadLibraryA("vulkan-1.dll");
        if (!vulkanLib) {
            return false;
        }
        
        auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanLib, "vkGetInstanceProcAddr");
        if (!vkGetInstanceProcAddr) {
            FreeLibrary(vulkanLib);
            return false;
        }
#else
        void* vulkanLib = dlopen("libvulkan.so", RTLD_LAZY);
        if (!vulkanLib) {
            return false;
        }
        
        auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vulkanLib, "vkGetInstanceProcAddr");
        if (!vkGetInstanceProcAddr) {
            dlclose(vulkanLib);
            return false;
        }
#endif

        auto vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(nullptr, "vkCreateInstance");
        if (!vkCreateInstance) {
#ifdef PLATFORM_WINDOWS
            FreeLibrary(vulkanLib);
#else
            dlclose(vulkanLib);
#endif
            return false;
        }

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "TCMT GPU Detection";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "TCMT Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        VkInstance instance;
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result == VK_SUCCESS) {
            gpu.supportsVulkan = true;
            
            uint32_t apiVersion;
            auto vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(instance, "vkEnumerateInstanceVersion");
            if (vkEnumerateInstanceVersion && vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS) {
                uint32_t major = VK_VERSION_MAJOR(apiVersion);
                uint32_t minor = VK_VERSION_MINOR(apiVersion);
                uint32_t patch = VK_VERSION_PATCH(apiVersion);
                gpu.vulkanVersion = std::to_wstring(major) + L"." + std::to_wstring(minor) + L"." + std::to_wstring(patch);
            } else {
                gpu.vulkanVersion = L"1.0.0";
            }

            auto vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
            auto vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
            
            if (vkEnumeratePhysicalDevices && vkGetPhysicalDeviceProperties) {
                uint32_t deviceCount = 0;
                vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
                
                if (deviceCount > 0) {
                    std::vector<VkPhysicalDevice> devices(deviceCount);
                    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
                    
                    for (const auto& device : devices) {
                        VkPhysicalDeviceProperties properties;
                        vkGetPhysicalDeviceProperties(device, &properties);
                        
                        std::string deviceName(properties.deviceName);
                        std::wstring deviceNameWide(deviceName.begin(), deviceName.end());
                        
                        if (gpu.name.find(deviceNameWide) != std::wstring::npos) {
                            uint32_t driverVersion = properties.driverVersion;
                            uint32_t major = VK_VERSION_MAJOR(driverVersion);
                            uint32_t minor = VK_VERSION_MINOR(driverVersion);
                            uint32_t patch = VK_VERSION_PATCH(driverVersion);
                            gpu.vulkanDriverVersion = std::to_wstring(major) + L"." + std::to_wstring(minor) + L"." + std::to_wstring(patch);
                            break;
                        }
                    }
                }
            }

            auto vkDestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddr(instance, "vkDestroyInstance");
            if (vkDestroyInstance) {
                vkDestroyInstance(instance, nullptr);
            }
        }

#ifdef PLATFORM_WINDOWS
        FreeLibrary(vulkanLib);
#else
        dlclose(vulkanLib);
#endif

        return gpu.supportsVulkan;
    }
    catch (...) {
    }
    return false;
}

// 添加缺失的方法实现
void GpuInfo::DetectGpusViaWmi() {
    if (!pSvc) {
        Logger::Error("WMI服务不可用");
        return;
    }
    
    QueryGpuInfo();
}

bool GpuInfo::IsNvidiaGpu(const std::wstring& name) {
    std::wstring lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
    
    return lowerName.find(L"nvidia") != std::wstring::npos ||
           lowerName.find(L"geforce") != std::wstring::npos ||
           lowerName.find(L"quadro") != std::wstring::npos ||
           lowerName.find(L"tesla") != std::wstring::npos ||
           lowerName.find(L"rtx") != std::wstring::npos ||
           lowerName.find(L"gtx") != std::wstring::npos;
}

bool GpuInfo::IsIntegratedGpu(const std::wstring& name) {
    std::wstring lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
    
    return lowerName.find(L"intel") != std::wstring::npos ||
           lowerName.find(L"radeon") != std::wstring::npos ||
           lowerName.find(L"amd") != std::wstring::npos && lowerName.find(L"integrated") != std::wstring::npos ||
           lowerName.find(L"integrated") != std::wstring::npos;
}

#ifdef CUDA_SUPPORTED
void GpuInfo::QueryNvidiaGpuDetails(GpuData& gpu) {
    // 这里应该实现NVML查询逻辑
    // 由于错误信息显示缺少这个方法，这里提供一个空实现
    Logger::Info("查询NVIDIA GPU详细信息: " + WinUtils::WstringToString(gpu.name));
}
#endif