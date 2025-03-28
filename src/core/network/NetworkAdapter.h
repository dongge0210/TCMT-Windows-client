#pragma once
#include <vector>
#include <string>
#include "WMIManager.h"

class NetworkAdapter {
public:
    NetworkAdapter(WmiManager& manager);
    struct AdapterInfo {
        std::wstring name;
        std::wstring ip;
        std::wstring mac;
    };

    NetworkAdapter();
    const std::vector<AdapterInfo>& GetAdapters() const;
    void ClearAdapters();
    void RefreshAdapters();

private:
    void QueryAdapterInfo();
    WmiManager& wmiManager;
    std::vector<AdapterInfo> adapters;
};