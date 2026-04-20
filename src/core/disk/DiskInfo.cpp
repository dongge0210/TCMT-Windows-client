// DiskInfo.cpp
#include "DiskInfo.h"
#include "../Utils/WinUtils.h"
#include "../Utils/Logger.h"
#include "../Utils/WmiManager.h"
#include <wbemidl.h>
#include <comdef.h>
#include <set>
#pragma comment(lib, "wbemuuid.lib")

DiskInfo::DiskInfo() { QueryDrives(); }

// 使用 LibreHardwareMonitor 收集 SMART 数据
void DiskInfo::CollectSmartData(SystemInfo& sysInfo) {
    // TODO: 实现 SMART 数据收集
    // 当前需要通过 C++/CLI 编译配置来支持 LibreHardwareMonitorBridge
    // 临时留空，等待配置修复
    Logger::Debug("Disk SMART data collection - placeholder");
}

void DiskInfo::QueryDrives() {
    drives.clear();
    DWORD driveMask = GetLogicalDrives();
    if (driveMask == 0) { Logger::Error("GetLogicalDrives 失败"); return; }
    for (int i = 0; i < 26; ++i) {
        if ((driveMask & (1 << i)) == 0) continue;
        char driveLetter = static_cast<char>('A' + i);
        if (driveLetter == 'A' || driveLetter == 'B') continue; // 跳过软驱
        std::wstring rootPath; rootPath.reserve(4); rootPath.push_back(static_cast<wchar_t>(L'A'+i)); rootPath.append(L":\\");
        UINT driveType = GetDriveTypeW(rootPath.c_str());
        if (!(driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE)) continue;
        ULARGE_INTEGER freeBytesAvailable{}; ULARGE_INTEGER totalBytes{}; ULARGE_INTEGER totalFreeBytes{};
        if (!GetDiskFreeSpaceExW(rootPath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) { Logger::Warn("GetDiskFreeSpaceEx 失败: " + WinUtils::WstringToString(rootPath)); continue; }
        if (totalBytes.QuadPart == 0) continue;
        DriveInfo info{}; info.letter = driveLetter; info.totalSize = totalBytes.QuadPart; info.freeSpace = totalFreeBytes.QuadPart; info.usedSpace = (totalBytes.QuadPart >= totalFreeBytes.QuadPart)? (totalBytes.QuadPart - totalFreeBytes.QuadPart):0ULL;
        // 获取卷标 / 文件系统
        wchar_t volumeName[MAX_PATH + 1] = {0};
        wchar_t fileSystemName[MAX_PATH + 1] = {0};
        DWORD fsFlags = 0;
        if (!GetVolumeInformationW(rootPath.c_str(), volumeName, MAX_PATH, nullptr, nullptr, &fsFlags, fileSystemName, MAX_PATH)) {
            info.label = L""; // 空表示未命名或获取失败
            info.fileSystem = L"未知";
            Logger::Warn("GetVolumeInformation 失败: " + WinUtils::WstringToString(rootPath));
        } else {
            info.label = volumeName;
            // 不再设置默认标签，让 UI 决定如何显示
            info.fileSystem = fileSystemName;
        }
        drives.push_back(std::move(info));
    }
    std::sort(drives.begin(), drives.end(), [](const DriveInfo& a,const DriveInfo& b){return a.letter<b.letter;});
}

void DiskInfo::Refresh() { QueryDrives(); }
const std::vector<DriveInfo>& DiskInfo::GetDrives() const { return drives; }

std::vector<DiskData> DiskInfo::GetDisks() {
    std::vector<DiskData> disks; disks.reserve(drives.size());
    for (const auto& drive : drives) { DiskData d; d.letter=drive.letter; d.totalSize=drive.totalSize; d.freeSpace=drive.freeSpace; d.usedSpace=drive.usedSpace; d.label=WinUtils::WstringToString(drive.label); d.fileSystem=WinUtils::WstringToString(drive.fileSystem); disks.push_back(std::move(d)); }
    return disks;
}

// ---------------- 物理磁盘 + 逻辑盘符映射实现 ----------------
// 使用 WMI 关联查询：Win32_DiskDrive -> Win32_DiskDriveToDiskPartition -> Win32_DiskPartition -> Win32_LogicalDiskToPartition -> Win32_LogicalDisk
static bool ParseDeviceID(const std::wstring& text, const wchar_t* prefix, int& indexOut) {
    // 解析 DeviceID 如 "\\.\PHYSICALDRIVE0" -> 提取 0
    size_t pos = text.find(prefix);
    if (pos != std::wstring::npos) {
        pos += wcslen(prefix);
        if (pos < text.size() && iswdigit(text[pos])) {
            indexOut = text[pos] - L'0';
            return true;
        }
    }
    return false;
}

void DiskInfo::CollectPhysicalDisks(WmiManager& wmi, const std::vector<DiskData>& logicalDisks, SystemInfo& sysInfo) {
    IWbemServices* svc = wmi.GetWmiService(); if (!svc) { Logger::Warn("WMI 服务无效，跳过物理磁盘枚举"); return; }
    std::map<int,std::vector<char>> physicalIndexToLetters;
    std::map<char, std::wstring> letterToLabel;
    
    // 先收集所有逻辑驱动器的信息（卷标等）
    {
        IEnumWbemClassObject* pEnum = nullptr;
        HRESULT hr = svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT DeviceID, VolumeName, Size FROM Win32_LogicalDisk"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
        if (SUCCEEDED(hr) && pEnum) {
            IWbemClassObject* obj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
                VARIANT vtDeviceID, vtVolumeName, vtSize;
                VariantInit(&vtDeviceID);
                VariantInit(&vtVolumeName);
                VariantInit(&vtSize);
                if (SUCCEEDED(obj->Get(L"DeviceID", 0, &vtDeviceID, 0, 0)) && vtDeviceID.vt == VT_BSTR) {
                    std::wstring deviceID = vtDeviceID.bstrVal;
                    if (deviceID.length() >= 2 && deviceID[1] == L':') {
                        char letter = static_cast<char>(::toupper(deviceID[0]));
                        bool hasValidSize = false;
                        if (SUCCEEDED(obj->Get(L"Size", 0, &vtSize, 0, 0))) {
                            if (vtSize.vt == VT_UI8 && vtSize.ullVal > 0) hasValidSize = true;
                            else if (vtSize.vt == VT_BSTR && vtSize.bstrVal) {
                                uint64_t size = _wcstoui64(vtSize.bstrVal, nullptr, 10);
                                if (size > 0) hasValidSize = true;
                            }
                        }
                        if (hasValidSize && letter >= 'A' && letter <= 'Z') {
                            if (SUCCEEDED(obj->Get(L"VolumeName", 0, &vtVolumeName, 0, 0)) && vtVolumeName.vt == VT_BSTR && vtVolumeName.bstrVal && vtVolumeName.bstrVal[0] != L'\0') {
                                letterToLabel[letter] = vtVolumeName.bstrVal;
                            } else {
                                wchar_t defaultLabel[32] = {};
                                swprintf_s(defaultLabel, L"%c:", letter);
                                letterToLabel[letter] = defaultLabel;
                            }
                        }
                    }
                }
                VariantClear(&vtDeviceID);
                VariantClear(&vtVolumeName);
                VariantClear(&vtSize);
                obj->Release();
            }
            pEnum->Release();
        }
    }
    
    // 使用关联查询获取物理磁盘到逻辑驱动器的映射
    // 对每个物理磁盘，查询关联的分区，再查询关联的逻辑驱动器
    {
        IEnumWbemClassObject* pEnum = nullptr;
        HRESULT hr = svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT DeviceID, Index FROM Win32_DiskDrive"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
        if (SUCCEEDED(hr) && pEnum) {
            IWbemClassObject* obj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
                VARIANT vtDeviceID, vtIndex;
                VariantInit(&vtDeviceID);
                VariantInit(&vtIndex);
                int diskIndex = -1;
                if (SUCCEEDED(obj->Get(L"Index", 0, &vtIndex, 0, 0)) && (vtIndex.vt == VT_I4 || vtIndex.vt == VT_UI4)) {
                    diskIndex = (vtIndex.vt == VT_I4) ? vtIndex.intVal : static_cast<int>(vtIndex.uintVal);
                }
                if (SUCCEEDED(obj->Get(L"DeviceID", 0, &vtDeviceID, 0, 0)) && vtDeviceID.vt == VT_BSTR && diskIndex >= 0) {
                    std::wstring diskDeviceID = vtDeviceID.bstrVal;
                    // 使用 ASSOCIATORS OF 查询关联的分区
                    std::wstring query = L"ASSOCIATORS OF {Win32_DiskDrive.DeviceID='";
                    query += diskDeviceID;
                    query += L"'} WHERE AssocClass = Win32_DiskDriveToDiskPartition";
                    IEnumWbemClassObject* pEnumPart = nullptr;
                    HRESULT hrPart = svc->ExecQuery(bstr_t(L"WQL"), bstr_t(query.c_str()), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumPart);
                    if (SUCCEEDED(hrPart) && pEnumPart) {
                        IWbemClassObject* objPart = nullptr;
                        ULONG retPart = 0;
                        while (pEnumPart->Next(WBEM_INFINITE, 1, &objPart, &retPart) == S_OK) {
                            VARIANT vtPartDeviceID;
                            VariantInit(&vtPartDeviceID);
                            if (SUCCEEDED(objPart->Get(L"DeviceID", 0, &vtPartDeviceID, 0, 0)) && vtPartDeviceID.vt == VT_BSTR) {
                                std::wstring partDeviceID = vtPartDeviceID.bstrVal;
                                // 再查询关联的逻辑驱动器
                                std::wstring queryLogical = L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='";
                                queryLogical += partDeviceID;
                                queryLogical += L"'} WHERE AssocClass = Win32_LogicalDiskToPartition";
                                IEnumWbemClassObject* pEnumLogical = nullptr;
                                HRESULT hrLogical = svc->ExecQuery(bstr_t(L"WQL"), bstr_t(queryLogical.c_str()), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumLogical);
                                if (SUCCEEDED(hrLogical) && pEnumLogical) {
                                    IWbemClassObject* objLogical = nullptr;
                                    ULONG retLogical = 0;
                                    while (pEnumLogical->Next(WBEM_INFINITE, 1, &objLogical, &retLogical) == S_OK) {
                                        VARIANT vtLogicalDeviceID;
                                        VariantInit(&vtLogicalDeviceID);
                                        if (SUCCEEDED(objLogical->Get(L"DeviceID", 0, &vtLogicalDeviceID, 0, 0)) && vtLogicalDeviceID.vt == VT_BSTR) {
                                            std::wstring logicalDeviceID = vtLogicalDeviceID.bstrVal;
                                            if (logicalDeviceID.length() >= 2 && logicalDeviceID[1] == L':') {
                                                char letter = static_cast<char>(::toupper(logicalDeviceID[0]));
                                                physicalIndexToLetters[diskIndex].push_back(letter);
                                            }
                                        }
                                        VariantClear(&vtLogicalDeviceID);
                                        objLogical->Release();
                                    }
                                    pEnumLogical->Release();
                                }
                            }
                            VariantClear(&vtPartDeviceID);
                            objPart->Release();
                        }
                        pEnumPart->Release();
                    }
                }
                VariantClear(&vtDeviceID);
                VariantClear(&vtIndex);
                obj->Release();
            }
            pEnum->Release();
        }
    }
    
    // 3. Win32_DiskDrive 基本信息
    std::map<int,PhysicalDiskSmartData> tempDisks; {
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT Index,Model,SerialNumber,InterfaceType,Size,MediaType FROM Win32_DiskDrive"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT vIndex,vModel,vSerial,vIface,vSize,vMedia; VariantInit(&vIndex);VariantInit(&vModel);VariantInit(&vSerial);VariantInit(&vIface);VariantInit(&vSize);VariantInit(&vMedia); if (SUCCEEDED(obj->Get(L"Index",0,&vIndex,0,0)) && (vIndex.vt==VT_I4||vIndex.vt==VT_UI4)){ int idx=(vIndex.vt==VT_I4)?vIndex.intVal:static_cast<int>(vIndex.uintVal); PhysicalDiskSmartData data{}; if (SUCCEEDED(obj->Get(L"Model",0,&vModel,0,0))&&vModel.vt==VT_BSTR) wcsncpy_s(data.model,vModel.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"SerialNumber",0,&vSerial,0,0))&&vSerial.vt==VT_BSTR) wcsncpy_s(data.serialNumber,vSerial.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"InterfaceType",0,&vIface,0,0))&&vIface.vt==VT_BSTR) wcsncpy_s(data.interfaceType,vIface.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"Size",0,&vSize,0,0))){ if (vSize.vt==VT_UI8) data.capacity=vSize.ullVal; else if (vSize.vt==VT_BSTR) data.capacity=_wcstoui64(vSize.bstrVal,nullptr,10);} if (SUCCEEDED(obj->Get(L"MediaType",0,&vMedia,0,0))&&vMedia.vt==VT_BSTR){ std::wstring media=vMedia.bstrVal; if (media.find(L"SSD")!=std::wstring::npos||media.find(L"Solid State")!=std::wstring::npos) wcsncpy_s(data.diskType,L"SSD",_TRUNCATE); else wcsncpy_s(data.diskType,L"HDD",_TRUNCATE);} else wcsncpy_s(data.diskType,L"未知",_TRUNCATE); data.smartSupported=false; data.smartEnabled=false; data.healthPercentage=0; data.temperature=0.0; data.logicalDriveCount=0; tempDisks[idx]=data; } VariantClear(&vIndex);VariantClear(&vModel);VariantClear(&vSerial);VariantClear(&vIface);VariantClear(&vSize);VariantClear(&vMedia); obj->Release(); } pEnum->Release(); } else { Logger::Warn("查询 Win32_DiskDrive 失败"); } }
    // 4. 填充盘符和卷标
    for (auto& kv: physicalIndexToLetters) {
        int diskIdx = kv.first;
        auto it = tempDisks.find(diskIdx);
        if (it == tempDisks.end()) continue;
        auto& pd = it->second;
        int count = 0;
        for (char L : kv.second) {
            if (count >= 8) break;
            pd.logicalDriveLetters[count] = L;
            // 填充卷标
            auto labelIt = letterToLabel.find(L);
            if (labelIt != letterToLabel.end()) {
                wcsncpy_s(pd.partitionLabels[count], labelIt->second.c_str(), _TRUNCATE);
            }
            count++;
        }
        pd.logicalDriveCount = count;
    }
    // 5. 过滤无效磁盘并写入 SystemInfo
    sysInfo.physicalDisks.clear();
    for (auto& kv: tempDisks) {
        const auto& disk = kv.second;
        // 过滤条件：容量为0、型号为空、型号包含Unknown、序列号为空
        if (disk.capacity == 0) continue;
        if (disk.model[0] == L'\0') continue;
        if (wcsstr(disk.model, L"Unknown") != nullptr) continue;
        if (disk.serialNumber[0] == L'\0') continue;
        sysInfo.physicalDisks.push_back(disk);
        if (sysInfo.physicalDisks.size() >= 8) break;
    }
}