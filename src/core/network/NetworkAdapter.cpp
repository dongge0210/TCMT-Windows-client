#include "NetworkAdapter.h"
#include "../Utils/Logger.h"
#include "../Utils/CrossPlatformSystemInfo.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

// ============================================================================
// NOTE:
// Original file contained duplicated definitions of Initialize(), QueryAdapterInfo(),
// QueryWmiAdapterInfo(), UpdateAdapterAddresses(), and various helpers for each platform.
// This caused redefinition compile errors. The file is refactored so each method is
// defined exactly once with platform conditionals inside or separate private helpers.
// ============================================================================

#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #include <comutil.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")
#elif defined(PLATFORM_MACOS)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <net/if_dl.h>
    #include <ifaddrs.h>
    #include <net/if_media.h>
    #include <sys/sysctl.h>
    #include <net/route.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/sockio.h>
    #include <sys/ioctl.h>
#elif defined(PLATFORM_LINUX)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <fstream>
    #include <sstream>
#endif

// ----------------------------------------------------------------------------
// Construction / Destruction
// ----------------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
NetworkAdapter::NetworkAdapter(WmiManager& manager)
    : wmiManager(manager), adapters(), initialized(false) {
    Logger::Debug("NetworkAdapter: Initializing (Windows)");
    auto& systemInfo = CrossPlatformSystemInfo::GetInstance();
    systemInfo.Initialize();
    Initialize();
}
#else
NetworkAdapter::NetworkAdapter() : adapters(), initialized(false) {
    Logger::Debug("NetworkAdapter: Initializing (Cross-Platform)");
    auto& systemInfo = CrossPlatformSystemInfo::GetInstance();
    systemInfo.Initialize();
    Initialize();
}
#endif

NetworkAdapter::~NetworkAdapter() {
    Cleanup();
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------
const std::vector<NetworkAdapter::AdapterInfo>& NetworkAdapter::GetAdapters() const {
    return adapters;
}

void NetworkAdapter::Refresh() {
    Logger::Debug("NetworkAdapter: Refresh requested");
    Cleanup();
    Initialize();
}

// ----------------------------------------------------------------------------
// Initialization / Cleanup
// ----------------------------------------------------------------------------
void NetworkAdapter::Initialize() {
    if (initialized) {
        return;
    }

    adapters.clear();

#ifdef PLATFORM_WINDOWS
    // Winsock startup (only once per process; OK to call again safely)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::Error("NetworkAdapter: WSAStartup failed");
        return;
    }
#endif

    QueryAdapterInfo();
    initialized = true;
}

void NetworkAdapter::Cleanup() {
    adapters.clear();
    initialized = false;
}

// ----------------------------------------------------------------------------
// Core Query (dispatch to platform helpers)
// ----------------------------------------------------------------------------
void NetworkAdapter::QueryAdapterInfo() {
    adapters.clear();

#ifdef PLATFORM_WINDOWS
    QueryWmiAdapterInfo();
    UpdateAdapterAddresses();
#else
    // Prefer higher level cross-platform manager if available
    auto& systemInfo = CrossPlatformSystemInfo::GetInstance();
    bool fromManager = false;
    if (systemInfo.Initialize()) {
        auto networkDevices = systemInfo.GetNetworkAdapters();
        for (const auto& device : networkDevices) {
            AdapterInfo adapter;
            adapter.name = device.name;
            adapter.description = device.description;
            adapter.mac = device.properties.count("mac_address") ? device.properties.at("mac_address") : "";
            if (device.properties.count("ip_address")) {
                adapter.ip = device.properties.at("ip_address");
                adapter.ipAddresses.push_back(adapter.ip);
            }
            if (device.properties.count("gateway")) {
                adapter.gateway = device.properties.at("gateway");
            }
            if (device.properties.count("speed")) {
                try {
                    adapter.speed = std::stoull(device.properties.at("speed"));
                    adapter.speedString = FormatSpeed(adapter.speed);
                } catch (...) {
                    adapter.speed = 0; adapter.speedString = "Unknown";
                }
            } else {
                adapter.speed = 0; adapter.speedString = "Unknown";
            }
            adapter.isEnabled = true;
            adapter.isConnected = (adapter.speed > 0);
            adapter.isVirtual = IsVirtualAdapter(adapter.name);
            adapter.isWireless = (adapter.name.rfind("wl",0)==0 || adapter.name.rfind("wlan",0)==0);
            adapter.connectionStatus = adapter.isConnected ? "Connected" : "Disconnected";
            adapter.mtu = 1500;
            adapter.bytesReceived = 0;
            adapter.bytesSent = 0;
            adapter.packetsReceived = 0;
            adapter.packetsSent = 0;
            adapters.push_back(adapter);
        }
        fromManager = !adapters.empty();
    }
    if (!fromManager) {
    #if defined(PLATFORM_MACOS)
        QueryMacNetworkAdapters();
        UpdateMacAdapterAddresses();
    #elif defined(PLATFORM_LINUX)
        QueryLinuxNetworkAdapters();
        UpdateLinuxAdapterAddresses();
    #endif
    }
#endif
}

// ============================================================================
// Windows Specific Implementation
// ============================================================================
#ifdef PLATFORM_WINDOWS

// Detect virtual adapters (wide-string)
bool NetworkAdapter::IsVirtualAdapter(const std::wstring& name) const {
    const std::vector<std::wstring> virtualPatterns = {
        L"Virtual", L"VMware", L"Hyper-V", L"VBox", L"QEMU", L"TAP", L"TUN",
        L"Loopback", L"Microsoft KM-TEST", L"Microsoft Hosted Network", L"Bluetooth",
        L"Wi-Fi Direct"
    };
    for (const auto& pattern : virtualPatterns) {
        if (name.find(pattern) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

void NetworkAdapter::QueryWmiAdapterInfo() {
    Logger::Debug("NetworkAdapter: WMI adapter query (Windows)");
    try {
        std::wstring query = L"SELECT * FROM Win32_NetworkAdapter WHERE PhysicalAdapter = True";
        IEnumWbemClassObject* pEnumerator = wmiManager.ExecuteQuery(query);
        if (!pEnumerator) {
            Logger::Error("NetworkAdapter: Failed to execute WMI query");
            return;
        }

        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        while (pEnumerator) {
            HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) break;

            VARIANT vtProp;
            VariantInit(&vtProp);

            AdapterInfo info{};
            
            // 获取名称
            hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                std::wstring name = vtProp.bstrVal;
                VariantClear(&vtProp);
                
                // 获取MAC地址
                hr = pclsObj->Get(L"MACAddress", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                    std::wstring mac = vtProp.bstrVal;
                    
                    if (!name.empty() && !mac.empty() && 
                        !IsVirtualAdapter(name) && !IsVirtualAdapter(mac)) {
                        
                        info.name = std::string(name.begin(), name.end());
                        info.mac = std::string(mac.begin(), mac.end());
                        
                        // 获取描述
                        VariantClear(&vtProp);
                        hr = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                            std::wstring description = vtProp.bstrVal;
                            info.description = std::string(description.begin(), description.end());
                        }
                        
                        // 获取启用状态
                        VariantClear(&vtProp);
                        hr = pclsObj->Get(L"NetEnabled", 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
                            info.isEnabled = (vtProp.boolVal == VARIANT_TRUE);
                            info.isConnected = info.isEnabled;
                        }
                        
                        info.adapterType = "Unknown";
                        info.speed = 0;
                        info.speedString = "Unknown";
                        info.mtu = 0;
                        info.bytesReceived = info.bytesSent = 0;
                        info.packetsReceived = info.packetsSent = 0;
                        info.isVirtual = false;
                        info.isWireless = false;
                        info.connectionStatus = info.isConnected ? "Connected" : "Disconnected";
                        
                        adapters.push_back(info);
                    }
                }
            }
            
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        pEnumerator->Release();
    } catch (const std::exception& e) {
        Logger::Error(std::string("NetworkAdapter: WMI query failed: ") + e.what());
    }
}

void NetworkAdapter::UpdateAdapterAddresses() {
    ULONG bufferSize = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, nullptr, &bufferSize) != ERROR_BUFFER_OVERFLOW) {
        return;
    }
    std::vector<BYTE> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, pAddresses, &bufferSize) != NO_ERROR) {
        Logger::Error("NetworkAdapter: GetAdaptersAddresses failed");
        return;
    }
    for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr; curr = curr->Next) {
        std::wstring friendly(curr->FriendlyName);
        std::string friendlyUtf8(friendly.begin(), friendly.end());
        for (auto& adapter : adapters) {
            if (adapter.name != friendlyUtf8) continue;
            // MAC (already set via WMI); update IP list
            adapter.ipAddresses.clear();
            for (PIP_ADAPTER_UNICAST_ADDRESS unicast = curr->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                int fam = unicast->Address.lpSockaddr->sa_family;
                if (fam == AF_INET) {
                    char ipStr[INET_ADDRSTRLEN];
                    auto ipv4 = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                    inet_ntop(AF_INET, &ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
                    adapter.ipAddresses.push_back(ipStr);
                    if (adapter.ip.empty()) adapter.ip = ipStr;
                } else if (fam == AF_INET6) {
                    char ipStr[INET6_ADDRSTRLEN];
                    auto ipv6 = reinterpret_cast<sockaddr_in6*>(unicast->Address.lpSockaddr);
                    inet_ntop(AF_INET6, &ipv6->sin6_addr, ipStr, INET6_ADDRSTRLEN);
                    adapter.ipv6Address = ipStr;
                }
            }
            for (PIP_ADAPTER_GATEWAY_ADDRESS gw = curr->FirstGatewayAddress; gw; gw = gw->Next) {
                if (gw->Address.lpSockaddr->sa_family == AF_INET) {
                    char ipStr[INET_ADDRSTRLEN];
                    auto ipv4 = reinterpret_cast<sockaddr_in*>(gw->Address.lpSockaddr);
                    inet_ntop(AF_INET, &ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
                    adapter.gateway = ipStr;
                }
            }
            adapter.mtu = curr->Mtu;
            if (curr->OperStatus == IfOperStatusUp) {
                MIB_IF_ROW2 row{}; row.InterfaceLuid = curr->Luid;
                if (GetIfEntry2(&row) == NO_ERROR) {
                    adapter.bytesReceived = row.InOctets;
                    adapter.bytesSent = row.OutOctets;
                    adapter.packetsReceived = row.InUcastPkts + row.InNUcastPkts;
                    adapter.packetsSent = row.OutUcastPkts + row.OutNUcastPkts;
                    adapter.speed = row.TransmitLinkSpeed; // bits per second
                    adapter.speedString = FormatSpeed(adapter.speed);
                    adapter.isConnected = true;
                    adapter.connectionStatus = "Connected";
                }
            } else {
                adapter.isConnected = false;
                adapter.connectionStatus = "Disconnected";
                adapter.speed = 0; adapter.speedString = "Not connected";
            }
        }
    }
}

std::wstring NetworkAdapter::FormatMacAddress(const unsigned char* addr, size_t len) const {
    std::wstringstream ss;
    for (size_t i=0;i<len;i++) {
        if (i) ss << L":";
        ss << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << (int)addr[i];
    }
    return ss.str();
}

std::wstring NetworkAdapter::FormatSpeed(uint64_t bps) const {
    const double KB=1e3, MB=1e6, GB=1e9;
    std::wstringstream ss; ss<<std::fixed<<std::setprecision(1);
    if (bps >= GB) ss << (bps/GB) << L" Gbps"; else if (bps >= MB) ss << (bps/MB) << L" Mbps"; else if (bps >= KB) ss << (bps/KB) << L" Kbps"; else ss << bps << L" bps";
    return ss.str();
}

std::wstring NetworkAdapter::DetermineAdapterType(const std::wstring& name, const std::wstring& desc, DWORD ifType) const {
    if (ifType == IF_TYPE_IEEE80211) return L"Wireless";
    if (ifType == IF_TYPE_ETHERNET_CSMACD) return L"Wired";
    std::wstring lower = name + L" " + desc; std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (lower.find(L"wifi")!=std::wstring::npos || lower.find(L"wireless")!=std::wstring::npos) return L"Wireless";
    if (lower.find(L"ethernet")!=std::wstring::npos || lower.find(L"gigabit")!=std::wstring::npos) return L"Wired";
    return L"Unknown";
}

void NetworkAdapter::SafeRelease(IUnknown* p) { if (p) p->Release(); }

#endif // PLATFORM_WINDOWS

// ============================================================================
// macOS Specific Implementation
// ============================================================================
#ifdef PLATFORM_MACOS

bool NetworkAdapter::IsVirtualAdapter(const std::string& name) const {
    const std::vector<std::string> virtualKeywords = {"lo","loopback","vmnet","veth","docker","br-","utun","awdl","p2p","llw","anpi"};
    for (const auto& k: virtualKeywords) if (name.find(k)!=std::string::npos) return true; return false;
}

std::string NetworkAdapter::FormatMacAddress(const unsigned char* address, size_t length) const {
    std::stringstream ss;
    for (size_t i = 0; i < length; i++) {
        if (i)
            ss << ":";
        ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)address[i];
    }
    return ss.str();
}

std::string NetworkAdapter::FormatSpeed(uint64_t bps) const {
    const double KB = 1e3, MB = 1e6, GB = 1e9;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bps >= GB)
        ss << (bps / GB) << " Gbps";
    else if (bps >= MB)
        ss << (bps / MB) << " Mbps";
    else if (bps >= KB)
        ss << (bps / KB) << " Kbps";
    else
        ss << bps << " bps";
    return ss.str();
}

std::string NetworkAdapter::DetermineAdapterType(const std::string& name, const std::string& /*desc*/, unsigned int /*ifType*/) const {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const std::vector<std::string> wifi = {"wi-fi", "wifi", "wireless", "wlan", "airport", "en0", "en1"};
    for (const auto& k : wifi)
        if (lower.find(k) != std::string::npos)
            return "Wireless";
    const std::vector<std::string> eth = {"ethernet", "gigabit", "en", "eth"};
    for (const auto& k : eth)
        if (lower.find(k) != std::string::npos)
            return "Wired";
    return "Unknown Type";
}

void NetworkAdapter::QueryMacNetworkAdapters() {
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        Logger::Error("NetworkAdapter: getifaddrs failed");
        return;
    }
    
    for (auto* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK)
            continue;
            
        AdapterInfo info{};
        info.name = ifa->ifa_name;
        info.isEnabled = (ifa->ifa_flags & (IFF_UP | IFF_RUNNING)) != 0;
        info.isConnected = info.isEnabled;
        
        struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
        if (sdl && sdl->sdl_alen == 6) {
            unsigned char* mac = (unsigned char*)LLADDR(sdl);
            info.mac = FormatMacAddress(mac, 6);
        }
        
        if (IsVirtualAdapter(info.name))
            continue;
            
        info.adapterType = DetermineAdapterType(info.name, "", 0);
        GetInterfaceStats(info.name, info);
        adapters.push_back(info);
    }
    
    freeifaddrs(ifap);
}

void NetworkAdapter::UpdateMacAdapterAddresses() {
    struct ifaddrs* ifap=nullptr; if (getifaddrs(&ifap)!=0){ Logger::Error("NetworkAdapter: getifaddrs failed (addr update)"); return; }
    for (auto* ifa=ifap; ifa; ifa=ifa->ifa_next){ if(!ifa->ifa_addr) continue; for (auto& adapter: adapters){ if (adapter.name==ifa->ifa_name){ if (ifa->ifa_addr->sa_family==AF_INET){ auto* addr_in=(struct sockaddr_in*)ifa->ifa_addr; char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&addr_in->sin_addr,ip,INET_ADDRSTRLEN); if (std::find(adapter.ipAddresses.begin(), adapter.ipAddresses.end(), ip)==adapter.ipAddresses.end()) adapter.ipAddresses.push_back(ip); if (adapter.ip.empty()) adapter.ip=ip; } else if (ifa->ifa_addr->sa_family==AF_INET6){ auto* addr_in6=(struct sockaddr_in6*)ifa->ifa_addr; char ip6[INET6_ADDRSTRLEN]; inet_ntop(AF_INET6,&addr_in6->sin6_addr,ip6,INET6_ADDRSTRLEN); adapter.ipv6Address=ip6; } } } } freeifaddrs(ifap); }

void NetworkAdapter::GetInterfaceStats(const std::string& interfaceName, AdapterInfo& info) const {
    int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0}; size_t len=0; if (sysctl(mib,6,nullptr,&len,nullptr,0)<0) return; std::vector<char> buffer(len); if (sysctl(mib,6,buffer.data(),&len,nullptr,0)<0) return; char* lim=buffer.data()+len; for(char* next=buffer.data(); next<lim; ){ auto* ifm=(struct if_msghdr*)next; next += ifm->ifm_msglen; if (ifm->ifm_type==RTM_IFINFO2){ auto* ifm2=(struct if_msghdr2*)ifm; char ifname[IF_NAMESIZE]; if_indextoname(ifm2->ifm_index, ifname); if (interfaceName==ifname){ if (ifm2->ifm_data.ifi_baudrate>0){ info.speed=ifm2->ifm_data.ifi_baudrate; info.speedString=FormatSpeed(info.speed);} break; } } } }

#endif // PLATFORM_MACOS

// ============================================================================
// Linux Specific Implementation
// ============================================================================
#ifdef PLATFORM_LINUX

bool NetworkAdapter::IsVirtualAdapter(const std::string& name) const {
    std::string lower=name; std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    const std::vector<std::string> patterns = {"virtual","vmware","hyper-v","vbox","qemu","tap","tun","loopback","docker","virbr","veth"};
    for (const auto& p: patterns) if (lower.find(p)!=std::string::npos) return true; return false;
}

std::string NetworkAdapter::FormatMacAddress(const unsigned char* address, size_t length) const {
    std::stringstream ss; for(size_t i=0;i<length;i++){ if(i) ss<<":"; ss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)address[i]; } return ss.str(); }
std::string NetworkAdapter::FormatSpeed(uint64_t bps) const { const double KB=1e3,MB=1e6,GB=1e9; std::stringstream ss; ss<<std::fixed<<std::setprecision(1); if (bps>=GB) ss<<(bps/GB)<<" Gbps"; else if (bps>=MB) ss<<(bps/MB)<<" Mbps"; else if (bps>=KB) ss<<(bps/KB)<<" Kbps"; else ss<<bps<<" bps"; return ss.str(); }

std::string NetworkAdapter::DetermineAdapterType(const std::string& name, const std::string& /*description*/, const std::string& /*driver*/) const {
    std::string lower=name; std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.rfind("wl",0)==0 || lower.rfind("wlan",0)==0) return "Wireless";
    if (lower.find("eth")!=std::string::npos) return "Wired"; return "Unknown Type";
}

void NetworkAdapter::QueryLinuxNetworkAdapters() {
    std::ifstream netDev("/proc/net/dev"); if (!netDev.is_open()){ Logger::Error("NetworkAdapter: cannot open /proc/net/dev"); return; }
    std::string line; std::getline(netDev,line); std::getline(netDev,line); // skip headers
    while (std::getline(netDev,line)){
        size_t colon=line.find(':'); if (colon==std::string::npos) continue; std::string iface=line.substr(0,colon); iface.erase(0, iface.find_first_not_of(" \t")); iface.erase(iface.find_last_not_of(" \t")+1);
        AdapterInfo info{}; info.name=iface; info.isEnabled=true; info.isVirtual=IsVirtualAdapter(iface); info.isWireless=(iface.rfind("wl",0)==0 || iface.rfind("wlan",0)==0);
        std::istringstream stats(line.substr(colon+1)); uint64_t rxBytes,rxPackets,rxErrs,rxDrop,rxFifo,rxFrame,rxCompressed,rxMulticast; uint64_t txBytes,txPackets,txErrs,txDrop,txFifo,txColls,txCarrier,txCompressed; stats>>rxBytes>>rxPackets>>rxErrs>>rxDrop>>rxFifo>>rxFrame>>rxCompressed>>rxMulticast>>txBytes>>txPackets>>txErrs>>txDrop>>txFifo>>txColls>>txCarrier>>txCompressed; info.bytesReceived=rxBytes; info.bytesSent=txBytes; info.packetsReceived=rxPackets; info.packetsSent=txPackets; info.isConnected=(rxBytes>0 || txBytes>0); info.connectionStatus=info.isConnected?"Connected":"Disconnected"; info.mtu=1500; info.speed=0; info.speedString="Unknown"; info.adapterType="Ethernet"; adapters.push_back(info);
    }
}

void NetworkAdapter::UpdateLinuxAdapterAddresses() {
    struct ifaddrs* ifap=nullptr; if (getifaddrs(&ifap)!=0) return; for(auto* ifa=ifap; ifa; ifa=ifa->ifa_next){ if(!ifa->ifa_addr) continue; std::string iface=ifa->ifa_name; for(auto& adapter: adapters){ if (adapter.name!=iface) continue; if (ifa->ifa_addr->sa_family==AF_INET){ auto* addr_in=(struct sockaddr_in*)ifa->ifa_addr; char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&addr_in->sin_addr,ip,INET_ADDRSTRLEN); if (std::find(adapter.ipAddresses.begin(), adapter.ipAddresses.end(), ip)==adapter.ipAddresses.end()) adapter.ipAddresses.push_back(ip); if (adapter.ip.empty()) adapter.ip=ip; if (ifa->ifa_netmask){ auto* mask_in=(struct sockaddr_in*)ifa->ifa_netmask; char maskStr[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&mask_in->sin_addr,maskStr,INET_ADDRSTRLEN); adapter.subnetMask=maskStr; } } else if (ifa->ifa_addr->sa_family==AF_INET6){ auto* addr_in6=(struct sockaddr_in6*)ifa->ifa_addr; char ip6[INET6_ADDRSTRLEN]; inet_ntop(AF_INET6,&addr_in6->sin6_addr,ip6,INET6_ADDRSTRLEN); adapter.ipv6Address=ip6; } } } freeifaddrs(ifap);
    // MTU & speed
    for(auto& adapter: adapters){ std::string mtuPath="/sys/class/net/"+adapter.name+"/mtu"; std::ifstream mtuFile(mtuPath); if(mtuFile.is_open()) mtuFile>>adapter.mtu; std::string speedPath="/sys/class/net/"+adapter.name+"/speed"; std::ifstream speedFile(speedPath); if(speedFile.is_open()){ speedFile>>adapter.speed; if(adapter.speed>0) adapter.speedString=FormatSpeed(adapter.speed*1000000); } }
    // Gateway (default route)
    std::ifstream routeFile("/proc/net/route"); if(routeFile.is_open()){ std::string rl; while(std::getline(routeFile, rl)){ std::istringstream iss(rl); std::string iface,destination,gateway; iss>>iface>>destination>>gateway; if(destination=="00000000"){ uint32_t gwHex; std::stringstream ss; ss<<std::hex<<gateway; ss>>gwHex; struct in_addr addr; addr.s_addr=gwHex; char gwStr[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&addr,gwStr,INET_ADDRSTRLEN); for(auto& adapter: adapters){ if(adapter.name==iface){ adapter.gateway=gwStr; break; } } } } }
}

#endif // PLATFORM_LINUX
