#include "NetworkAdapter.h"
#include <comutil.h>
#include <sstream>
#include "Logger.h"

NetworkAdapter::NetworkAdapter(WmiManager& manager) : wmiManager(manager) {
    if (wmiManager.IsInitialized()) {
        QueryAdapterInfo();
    }
    else {
        Logger::Error("WMI未初始化，无法获取网络信息");
    }
}

void NetworkAdapter::QueryAdapterInfo() {
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hres = wmiManager.GetWmiService()->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_NetworkAdapter WHERE NetConnectionStatus = 2"), // 状态2表示已连接
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator