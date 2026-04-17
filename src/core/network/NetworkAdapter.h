#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef TCMT_WINDOWS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

#ifdef TCMT_MACOS
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <map>
#endif

class WmiManager;

class NetworkAdapter {
public:
    struct AdapterInfo {
        std::string name;
        std::string mac;
        std::string ip;
        std::string description;
        std::string adapterType; // 无线/有线
        bool isEnabled;
        bool isConnected;
        uint64_t speed;
        std::string speedString;
    };

#ifdef TCMT_WINDOWS
    explicit NetworkAdapter(WmiManager& manager);
#elif defined(TCMT_MACOS)
    NetworkAdapter();
#endif
    ~NetworkAdapter();

    const std::vector<AdapterInfo>& GetAdapters() const;
    void Refresh();

private:
    void Initialize();
    void Cleanup();
    void QueryAdapterInfo();
    void UpdateAdapterAddresses();
    std::string FormatMacAddress(const unsigned char* address, size_t length) const;
    std::string FormatSpeed(uint64_t bitsPerSecond) const;
    bool IsVirtualAdapter(const std::string& name) const;

#ifdef TCMT_WINDOWS
    void QueryWmiAdapterInfo();
    std::string DetermineAdapterType(const std::wstring& name, const std::wstring& description, DWORD ifType) const;
    void SafeRelease(IUnknown* pInterface);
    WmiManager& wmiManager;
#endif

    std::vector<AdapterInfo> adapters;
    bool initialized;
};
