#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include "WmiManager.h"

class NetworkAdapter {
public:
    struct AdapterInfo {
        std::wstring name;
        std::wstring mac;
        std::wstring ip;
        std::wstring description;
        bool isEnabled;
        bool isConnected;
        uint64_t speed;
        std::wstring speedString;
    };

    explicit NetworkAdapter(WmiManager& manager);
    ~NetworkAdapter();

    const std::vector<AdapterInfo>& GetAdapters() const;
    void Refresh();

private:
    void Initialize();
    void Cleanup();
    void QueryAdapterInfo();
    void QueryWmiAdapterInfo();
    void UpdateAdapterAddresses();
    std::wstring FormatMacAddress(const unsigned char* address, size_t length) const;
    std::wstring FormatSpeed(uint64_t bitsPerSecond) const;  // 添加声明
    bool IsVirtualAdapter(const std::wstring& name) const;
    void SafeRelease(IUnknown* pInterface);

    WmiManager& wmiManager;
    std::vector<AdapterInfo> adapters;
    bool initialized;
};
