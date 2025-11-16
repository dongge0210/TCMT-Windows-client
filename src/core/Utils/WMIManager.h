#pragma once
#include <string>

#ifdef PLATFORM_WINDOWS
#include <wbemidl.h>
#include <comdef.h>
#include <servprov.h>
#endif

#ifdef PLATFORM_WINDOWS
class WmiManager : public IServiceProvider {
public:
    WmiManager();
    ~WmiManager();
    bool IsInitialized() const;

    // IServiceProvider接口实现
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryService(REFGUID guid, REFIID riid, void** ppv) override;

    IWbemServices* GetWmiService() const;
    
    // 执行WMI查询
    IEnumWbemClassObject* ExecuteQuery(const std::wstring& query);

private:
    void Initialize();
    void Cleanup();

    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    bool initialized = false;
    LONG m_refCount = 1;
};
#endif