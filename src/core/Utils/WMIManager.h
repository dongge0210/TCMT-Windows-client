#pragma once
#include <wbemidl.h>
#include <comdef.h>
#include <servprov.h>
#include <string>

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

private:
    void LogCOMError(HRESULT hr, const std::string& context);

    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    bool initialized = false;
    LONG m_refCount = 1;
};                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               