#pragma once
#include "../common/PlatformDefs.h"
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
// Platform includes kept minimal in header to reduce macro collision.
// Detailed system headers moved to .cpp.
//-----------------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
    #include "WmiManager.h"
#endif

class NetworkAdapter {
public:
    struct AdapterInfo {
        std::string name;
        std::string mac;
        std::string ip;
        std::string description;
        std::string adapterType;
        bool        isEnabled{};
        bool        isConnected{};
        uint64_t    speed{};
        std::string speedString;

        // Cross-platform extended fields
        std::vector<std::string> ipAddresses;  // multiple IPs
        std::string gateway;
        std::string subnetMask;
        std::string dnsServers;
        std::string ipv6Address;
        std::string ipv6Gateway;
        std::string dnsServersIPv6;
        uint64_t bytesReceived{};
        uint64_t bytesSent{};
        uint64_t packetsReceived{};
        uint64_t packetsSent{};
        uint32_t mtu{};
        std::string driverVersion;
        std::string firmwareVersion;
        bool isVirtual{};
        bool isWireless{};
        std::string connectionStatus;
    };

#ifdef PLATFORM_WINDOWS
    explicit NetworkAdapter(WmiManager& manager);
#else
    NetworkAdapter();
#endif
    ~NetworkAdapter();

    const std::vector<AdapterInfo>& GetAdapters() const;
    void Refresh();

private:
    void Initialize();
    void Cleanup();
    void QueryAdapterInfo();

#ifdef PLATFORM_WINDOWS
    void QueryWmiAdapterInfo();
    void UpdateAdapterAddresses();
    std::wstring FormatMacAddress(const unsigned char* address, size_t length) const;
    std::wstring FormatSpeed(uint64_t bitsPerSecond) const;
    bool IsVirtualAdapter(const std::wstring& name) const;
    void SafeRelease(IUnknown* pInterface);
    std::wstring DetermineAdapterType(const std::wstring& name, const std::wstring& description, DWORD ifType) const;
    WmiManager& wmiManager;
#elif defined(PLATFORM_MACOS)
    void QueryMacNetworkAdapters();
    void UpdateMacAdapterAddresses();
    std::string FormatMacAddress(const unsigned char* address, size_t length) const;
    std::string FormatSpeed(uint64_t bitsPerSecond) const;
    bool IsVirtualAdapter(const std::string& name) const;
    std::string DetermineAdapterType(const std::string& name, const std::string& description, unsigned int ifType) const;
    void GetInterfaceStats(const std::string& interfaceName, AdapterInfo& info) const;
#elif defined(PLATFORM_LINUX)
    void QueryLinuxNetworkAdapters();
    void UpdateLinuxAdapterAddresses();
    std::string FormatMacAddress(const unsigned char* address, size_t length) const;
    std::string FormatSpeed(uint64_t bitsPerSecond) const;
    bool IsVirtualAdapter(const std::string& name) const;
    std::string DetermineAdapterType(const std::string& name, const std::string& description, const std::string& driver) const;
#endif

    std::vector<AdapterInfo> adapters;
    bool initialized{};
};
