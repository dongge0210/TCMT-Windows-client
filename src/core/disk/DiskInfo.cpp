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
            if (info.label.empty()) info.label = L"未命名"; // 兜底
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

// ---------------- 物理磁盘 + 逻辑盘符映射实现合并 ----------------
static bool ParseDiskPartition(const std::wstring& text, int& diskIndexOut) {
    size_t posDisk = text.find(L"Disk #"); if (posDisk==std::wstring::npos) return false; posDisk += 6; if (posDisk>=text.size()) return false; int num=0; bool any=false; while (posDisk<text.size() && iswdigit(text[posDisk])) { any=true; num = num*10 + (text[posDisk]-L'0'); ++posDisk; } if(!any) return false; diskIndexOut = num; return true; }

void DiskInfo::CollectPhysicalDisks(WmiManager& wmi, const std::vector<DiskData>& logicalDisks, SystemInfo& sysInfo) {
    IWbemServices* svc = wmi.GetWmiService(); if (!svc) { Logger::Warn("WMI 服务无效，跳过物理磁盘枚举"); return; }
    std::map<int,std::vector<char>> physicalIndexToLetters;
    std::map<char,int> letterToDiskIndex;
    std::map<char, std::wstring> letterToLabel;  // 盘符 -> 卷标
    std::set<char> validLogicalDrives;  // 有效逻辑驱动器（容量>0）
    
    // 1. Win32_DiskDriveToDiskPartition
    {
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_DiskDriveToDiskPartition"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT ant; VARIANT dep; VariantInit(&ant); VariantInit(&dep); if (SUCCEEDED(obj->Get(L"Antecedent",0,&ant,0,0)) && ant.vt==VT_BSTR && SUCCEEDED(obj->Get(L"Dependent",0,&dep,0,0)) && dep.vt==VT_BSTR){ int diskIdx=-1; if (ParseDiskPartition(dep.bstrVal,diskIdx)){ /* mapping captured via next query */ } } VariantClear(&ant); VariantClear(&dep); obj->Release(); } pEnum->Release(); } else { Logger::Warn("查询 Win32_DiskDriveToDiskPartition 失败"); }
    }
    // 2. Win32_LogicalDiskToPartition 建立盘符 -> 物理索引
    // 先查询 Win32_LogicalDisk 获取所有逻辑驱动器的卷标和有效盘符
    {
        IEnumWbemClassObject* pEnumLabel = nullptr;
        HRESULT hrLabel = svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT DeviceID, VolumeName, Size FROM Win32_LogicalDisk"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumLabel);
        if (SUCCEEDED(hrLabel) && pEnumLabel) {
            IWbemClassObject* objLabel = nullptr;
            ULONG retLabel = 0;
            while (pEnumLabel->Next(WBEM_INFINITE, 1, &objLabel, &retLabel) == S_OK) {
                VARIANT vtDeviceID, vtVolumeName, vtSize;
                VariantInit(&vtDeviceID);
                VariantInit(&vtVolumeName);
                VariantInit(&vtSize);
                if (SUCCEEDED(objLabel->Get(L"DeviceID", 0, &vtDeviceID, 0, 0)) && vtDeviceID.vt == VT_BSTR) {
                    std::wstring deviceID = vtDeviceID.bstrVal;
                    if (deviceID.length() >= 2) {
                        char letter = static_cast<char>(::toupper(deviceID[0]));
                        
                        // 检查是否有有效容量（过滤无效分区）
                        bool hasValidSize = false;
                        if (SUCCEEDED(objLabel->Get(L"Size", 0, &vtSize, 0, 0))) {
                            if (vtSize.vt == VT_UI8 && vtSize.ullVal > 0) {
                                hasValidSize = true;
                            } else if (vtSize.vt == VT_BSTR && vtSize.bstrVal) {
                                uint64_t size = _wcstoui64(vtSize.bstrVal, nullptr, 10);
                                if (size > 0) hasValidSize = true;
                            }
                        }
                        
                        // 只记录有效分区（有容量且盘符有效）
                        if (hasValidSize && letter >= 'A' && letter <= 'Z') {
                            // 获取卷标，如果没有卷标则使用盘符作为默认标签
                            if (SUCCEEDED(objLabel->Get(L"VolumeName", 0, &vtVolumeName, 0, 0)) && vtVolumeName.vt == VT_BSTR && vtVolumeName.bstrVal && vtVolumeName.bstrVal[0] != L'\0') {
                                letterToLabel[letter] = vtVolumeName.bstrVal;
                            } else {
                                // 没有卷标时，使用盘符作为默认标签
                                wchar_t defaultLabel[32] = {};
                                swprintf_s(defaultLabel, L"%c:", letter);
                                letterToLabel[letter] = defaultLabel;
                            }
                            // 记录有效盘符，用于后续过滤无效分区
                            validLogicalDrives.insert(letter);
                        }
                    }
                }
                VariantClear(&vtDeviceID);
                VariantClear(&vtVolumeName);
                VariantClear(&vtSize);
                objLabel->Release();
            }
            pEnumLabel->Release();
        }
        // 再查询 Win32_LogicalDiskToPartition 获取盘符与物理磁盘的对应关系
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_LogicalDiskToPartition"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT ant; VARIANT dep; VariantInit(&ant); VariantInit(&dep); if (SUCCEEDED(obj->Get(L"Antecedent",0,&ant,0,0)) && ant.vt==VT_BSTR && SUCCEEDED(obj->Get(L"Dependent",0,&dep,0,0)) && dep.vt==VT_BSTR){ int diskIdx=-1; if (ParseDiskPartition(ant.bstrVal,diskIdx)){ std::wstring depStr = dep.bstrVal; size_t pos = depStr.find(L"DeviceID=\""); if (pos!=std::wstring::npos){ pos+=10; if (pos<depStr.size()){ wchar_t letterW = depStr[pos]; if(letterW && letterW!=L'"'){ letterToDiskIndex[ static_cast<char>(::toupper(letterW)) ] = diskIdx; } } } } } VariantClear(&ant); VariantClear(&dep); obj->Release(); } pEnum->Release(); } else { Logger::Warn("查询 Win32_LogicalDiskToPartition 失败"); } 
    }
    for (auto& kv: letterToDiskIndex) physicalIndexToLetters[kv.second].push_back(kv.first);
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