#include "WmiManager.h"
#include "Logger.h"
#include <comdef.h>

WmiManager::WmiManager() {
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        Logger::Error("COM初始化失败: 0x" + std::to_string(hres));
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
        Logger::Error("安全初始化失败: 0x" + std::to_string(hres));
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
        Logger::Error("创建WMI定位器失败: 0x" + std::to_string(hres));
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
        Logger::Error("连接WMI命名空间失败: 0x" + std::to_string(hres));
        pLoc->Release();
        CoUninitialize();
        return;
    }

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
        Logger::Error("设置代理权限失败: 0x" + std::to_string(hres));
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    initialized = true;
}

WmiManager::~WmiManager() {
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();
}

bool WmiManager::IsInitialized() const {
    return initialized;
}

IWbemServices* WmiManager::GetWmiService() const {
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