#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "NetworkAdapter.h"
#include "Logger.h"
#include <comutil.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

NetworkAdapter::NetworkAdapter(WmiManager& manager)
    : wmiManager(manager), initialized(false) {
    Initialize();
}

NetworkAdapter::~NetworkAdapter() {
    Cleanup();
}

void NetworkAdapter::Initialize() {
    if (wmiManager.IsInitialized()) {
        QueryAdapterInfo();
        initialized = true;
    }
    else {
        Logger::Error("WMI未初始化，无法获取网络信息");
    }
}

void NetworkAdapter::Cleanup() {
    adapters.clear();
    initialized = false;
}

void NetworkAdapter::Refresh() {
    Cleanup();
    Initialize();
}

void NetworkAdapter::QueryAdapterInfo() {
    QueryWmiAdapterInfo();
    UpdateAdapterAddresses();
}

bool NetworkAdapter::IsVirtualAdapter(const std::wstring& name) const {
    const std::wstring virtualKeywords[] = {
        L"VirtualBox",
        L"Hyper-V",
        L"Virtual",
        L"VPN",
        L"Bluetooth",
        L"VMware",
        L"Loopback",
        L"Microsoft Wi-Fi Direct"
    };

    for (const auto& keyword : virtualKeywords) {
        if (name.find(keyword) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

void NetworkAdapter::QueryWmiAdapterInfo() {
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hres = wmiManager.GetWmiService()->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_NetworkAdapter WHERE PhysicalAdapter = True"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );

    if (FAILED(hres)) {
        Logger::Error("网络适配器WMI查询失败: HRESULT=0x" + std::to_string(hres));
        return;
    }

    ULONG uReturn = 0;
    IWbemClassObject* pclsObj = nullptr;
    while (pEnumerator && pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK) {
        AdapterInfo info;
        info.isEnabled = false;

        // 获取适配器名称
        VARIANT vtName, vtDesc, vtStatus;
        VariantInit(&vtName);
        VariantInit(&vtDesc);
        VariantInit(&vtStatus);

        if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtName, 0, 0)) && vtName.vt == VT_BSTR) {
            info.name = vtName.bstrVal;
        }

        if (SUCCEEDED(pclsObj->Get(L"Description", 0, &vtDesc, 0, 0)) && vtDesc.vt == VT_BSTR) {
            info.description = vtDesc.bstrVal;
        }

        // 检查适配器状态
        if (SUCCEEDED(pclsObj->Get(L"NetEnabled", 0, &vtStatus, 0, 0))) {
            info.isEnabled = (vtStatus.boolVal == VARIANT_TRUE);
        }

        VariantClear(&vtName);
        VariantClear(&vtDesc);
        VariantClear(&vtStatus);

        // 过滤虚拟适配器
        if (IsVirtualAdapter(info.name) || IsVirtualAdapter(info.description)) {
            SafeRelease(pclsObj);
            continue;
        }

        // 获取MAC地址
        VARIANT vtMac;
        VariantInit(&vtMac);
        if (SUCCEEDED(pclsObj->Get(L"MACAddress", 0, &vtMac, 0, 0)) && vtMac.vt == VT_BSTR) {
            info.mac = vtMac.bstrVal;
        }
        VariantClear(&vtMac);

        if (!info.name.empty() && !info.mac.empty()) {
            adapters.push_back(info);
        }

        SafeRelease(pclsObj);
    }

    SafeRelease(pEnumerator);
}

std::wstring NetworkAdapter::FormatMacAddress(const unsigned char* address, size_t length) const {
    std::wstringstream ss;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0) ss << L":";
        ss << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
            << static_cast<int>(address[i]);
    }
    return ss.str();
}

void NetworkAdapter::UpdateAdapterAddresses() {
    ULONG bufferSize = 15000;
    std::vector<BYTE> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer[0]);

    DWORD result = GetAdaptersAddresses(AF_INET,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,  // 添加网关信息
        nullptr,
        pAddresses,
        &bufferSize);

    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer[0]);
        result = GetAdaptersAddresses(AF_INET,
            GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
            nullptr,
            pAddresses,
            &bufferSize);
    }

    if (result != NO_ERROR) {
        Logger::Error("获取网络适配器地址失败: " + std::to_string(result));
        return;
    }

    for (PIP_ADAPTER_ADDRESSES adapter = pAddresses; adapter; adapter = adapter->Next) {
        if (adapter->IfType != IF_TYPE_ETHERNET_CSMACD &&
            adapter->IfType != IF_TYPE_IEEE80211) {
            continue;
        }

        std::wstring macAddress = FormatMacAddress(
            adapter->PhysicalAddress,
            adapter->PhysicalAddressLength);

        for (auto& adapterInfo : adapters) {
            if (adapterInfo.mac == macAddress) {
                // 更新连接状态
                adapterInfo.isConnected = (adapter->OperStatus == IfOperStatusUp);

                // 更新网络速度
                adapterInfo.speed = adapter->TransmitLinkSpeed;
                adapterInfo.speedString = FormatSpeed(adapter->TransmitLinkSpeed);

                // 更新IP地址（仅当连接时）
                if (adapterInfo.isConnected) {
                    PIP_ADAPTER_UNICAST_ADDRESS address = adapter->FirstUnicastAddress;
                    while (address) {
                        if (address->Address.lpSockaddr->sa_family == AF_INET) {
                            char ipStr[INET_ADDRSTRLEN];
                            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
                            adapterInfo.ip = std::wstring(ipStr, ipStr + strlen(ipStr));
                            break;
                        }
                        address = address->Next;
                    }
                }
                else {
                    adapterInfo.ip = L"未连接";
                }
                break;
            }
        }
    }
}

// 添加格式化网络速度的辅助方法
std::wstring NetworkAdapter::FormatSpeed(uint64_t bitsPerSecond) const {  // 添加 const
    const double GB = 1000000000.0;
    const double MB = 1000000.0;
    const double KB = 1000.0;

    std::wstringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (bitsPerSecond >= GB) {
        ss << (bitsPerSecond / GB) << L" Gbps";
    }
    else if (bitsPerSecond >= MB) {
        ss << (bitsPerSecond / MB) << L" Mbps";
    }
    else if (bitsPerSecond >= KB) {
        ss << (bitsPerSecond / KB) << L" Kbps";
    }
    else {
        ss << bitsPerSecond << L" bps";
    }

    return ss.str();
}


void NetworkAdapter::SafeRelease(IUnknown* pInterface) {
    if (pInterface) {
        pInterface->Release();
    }
}

const std::vector<NetworkAdapter::AdapterInfo>& NetworkAdapter::GetAdapters() const {
    return adapters;
}
