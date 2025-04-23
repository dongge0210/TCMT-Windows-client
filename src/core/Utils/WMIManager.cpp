#include "WmiManager.h"
#include "Logger.h"
#include <comdef.h>

WmiManager::WmiManager() : initialized(false), pLoc(nullptr), pSvc(nullptr) {
    Initialize();
}

WmiManager::~WmiManager() {
    Cleanup();
}

void WmiManager::Initialize() {
    // 已由main.cpp统一初始化COM，此处仅需安全验证
    HRESULT hres = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    // 允许安全初始化已完成的情况
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        Logger::Error("安全初始化失败: 0x" + std::to_string(hres));
        return;
    }

    // 创建WMI定位器
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hres)) {
        Logger::Error("创建WMI定位器失败: 0x" + std::to_string(hres));
        return;
    }

    // 连接WMI服务
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hres)) {
        Logger::Error("连接WMI命名空间失败: 0x" + std::to_string(hres));
        Cleanup();
        return;
    }

    // 设置代理安全级别
    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hres)) {
        Logger::Error("设置代理安全失败: 0x" + std::to_string(hres));
        Cleanup();
        return;
    }

    initialized = true;
}

void WmiManager::Cleanup() {
    if (pSvc) {
        pSvc->Release();
        pSvc = nullptr;
    }
    if (pLoc) {
        pLoc->Release();
        pLoc = nullptr;
    }
    CoUninitialize();
    initialized = false;
}

bool WmiManager::IsInitialized() const {
    return initialized;
}

IWbemServices* WmiManager::GetWmiService() const {
    if (!initialized) {
        Logger::Error("尝试获取WMI服务时，WMI管理器未初始化");
        return nullptr;
    }
    return pSvc;
}

HRESULT STDMETHODCALLTYPE WmiManager::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IServiceProvider) {
        *ppvObject = static_cast<IServiceProvider*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WmiManager::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE WmiManager::Release() {
    ULONG refCount = InterlockedDecrement(&m_refCount);
    if (refCount == 0) delete this;
    return refCount;
}

HRESULT STDMETHODCALLTYPE WmiManager::QueryService(REFGUID guidService, REFIID riid, void** ppvObject) {
    if (guidService == IID_IWbemServices) {
        return pSvc->QueryInterface(riid, ppvObject);
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}
