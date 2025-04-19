#include "WmiManager.h"
#include "Logger.h"
#include <comdef.h>

WmiManager::WmiManager() : initialized(false), pLoc(nullptr), pSvc(nullptr) {
    Initialize(); // Ensure Initialize is called
}

WmiManager::~WmiManager() {
    Cleanup(); // Ensure Cleanup is called
}

void WmiManager::Initialize() {
    HRESULT hres = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hres)) {
        Logger::Error("COM initialization failed: 0x" + std::to_string(hres));
        return;
    }

    hres = CoInitializeSecurity(
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

    if (FAILED(hres)) {
        Logger::Error("Security initialization failed: 0x" + std::to_string(hres));
        CoUninitialize();
        return;
    }

    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hres)) {
        Logger::Error("Failed to create WMI locator: 0x" + std::to_string(hres));
        CoUninitialize();
        return;
    }

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
        Logger::Error("Failed to connect to WMI namespace: 0x" + std::to_string(hres));
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
