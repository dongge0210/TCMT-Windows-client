// DiskInfo.cpp
#include "DiskInfo.h"
#include "../Utils/WinUtils.h"
#include "../Utils/Logger.h"
#include "../Utils/WmiManager.h"
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")

DiskInfo::DiskInfo() { QueryDrives(); }

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
    // 1. Win32_DiskDriveToDiskPartition
    {
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_DiskDriveToDiskPartition"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT ant; VARIANT dep; VariantInit(&ant); VariantInit(&dep); if (SUCCEEDED(obj->Get(L"Antecedent",0,&ant,0,0)) && ant.vt==VT_BSTR && SUCCEEDED(obj->Get(L"Dependent",0,&dep,0,0)) && dep.vt==VT_BSTR){ int diskIdx=-1; if (ParseDiskPartition(dep.bstrVal,diskIdx)){ /* mapping captured via next query */ } } VariantClear(&ant); VariantClear(&dep); obj->Release(); } pEnum->Release(); } else { Logger::Warn("查询 Win32_DiskDriveToDiskPartition 失败"); }
    }
    // 2. Win32_LogicalDiskToPartition 建立盘符 -> 物理索引
    std::map<char,int> letterToDiskIndex; {
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_LogicalDiskToPartition"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT ant; VARIANT dep; VariantInit(&ant); VariantInit(&dep); if (SUCCEEDED(obj->Get(L"Antecedent",0,&ant,0,0)) && ant.vt==VT_BSTR && SUCCEEDED(obj->Get(L"Dependent",0,&dep,0,0)) && dep.vt==VT_BSTR){ int diskIdx=-1; if (ParseDiskPartition(ant.bstrVal,diskIdx)){ std::wstring depStr = dep.bstrVal; size_t pos = depStr.find(L"DeviceID=\""); if (pos!=std::wstring::npos){ pos+=10; if (pos<depStr.size()){ wchar_t letterW = depStr[pos]; if(letterW && letterW!=L'"'){ letterToDiskIndex[ static_cast<char>(::toupper(letterW)) ] = diskIdx; } } } } } VariantClear(&ant); VariantClear(&dep); obj->Release(); } pEnum->Release(); } else { Logger::Warn("查询 Win32_LogicalDiskToPartition 失败"); } }
    for (auto& kv: letterToDiskIndex) physicalIndexToLetters[kv.second].push_back(kv.first);
    // 3. Win32_DiskDrive 基本信息
    std::map<int,PhysicalDiskSmartData> tempDisks; {
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT Index,Model,SerialNumber,InterfaceType,Size,MediaType FROM Win32_DiskDrive"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT vIndex,vModel,vSerial,vIface,vSize,vMedia; VariantInit(&vIndex);VariantInit(&vModel);VariantInit(&vSerial);VariantInit(&vIface);VariantInit(&vSize);VariantInit(&vMedia); if (SUCCEEDED(obj->Get(L"Index",0,&vIndex,0,0)) && (vIndex.vt==VT_I4||vIndex.vt==VT_UI4)){ int idx=(vIndex.vt==VT_I4)?vIndex.intVal:static_cast<int>(vIndex.uintVal); PhysicalDiskSmartData data{}; if (SUCCEEDED(obj->Get(L"Model",0,&vModel,0,0))&&vModel.vt==VT_BSTR) wcsncpy_s(data.model,vModel.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"SerialNumber",0,&vSerial,0,0))&&vSerial.vt==VT_BSTR) wcsncpy_s(data.serialNumber,vSerial.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"InterfaceType",0,&vIface,0,0))&&vIface.vt==VT_BSTR) wcsncpy_s(data.interfaceType,vIface.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"Size",0,&vSize,0,0))){ if (vSize.vt==VT_UI8) data.capacity=vSize.ullVal; else if (vSize.vt==VT_BSTR) data.capacity=_wcstoui64(vSize.bstrVal,nullptr,10);} if (SUCCEEDED(obj->Get(L"MediaType",0,&vMedia,0,0))&&vMedia.vt==VT_BSTR){ std::wstring media=vMedia.bstrVal; if (media.find(L"SSD")!=std::wstring::npos||media.find(L"Solid State")!=std::wstring::npos) wcsncpy_s(data.diskType,L"SSD",_TRUNCATE); else wcsncpy_s(data.diskType,L"HDD",_TRUNCATE);} else wcsncpy_s(data.diskType,L"未知",_TRUNCATE); data.smartSupported=false; data.smartEnabled=false; data.healthPercentage=0; data.temperature=0.0; data.logicalDriveCount=0; tempDisks[idx]=data; } VariantClear(&vIndex);VariantClear(&vModel);VariantClear(&vSerial);VariantClear(&vIface);VariantClear(&vSize);VariantClear(&vMedia); obj->Release(); } pEnum->Release(); } else { Logger::Warn("查询 Win32_DiskDrive 失败"); } }
    // 4. 填充盘符
    for (auto& kv: physicalIndexToLetters){ int diskIdx=kv.first; auto it=tempDisks.find(diskIdx); if(it==tempDisks.end()) continue; auto& pd=it->second; int count=0; for(char L: kv.second){ if(count>=8) break; pd.logicalDriveLetters[count++]=L; } pd.logicalDriveCount=count; }
    // 5. 写入 SystemInfo
    sysInfo.physicalDisks.clear(); for (auto& kv: tempDisks){ sysInfo.physicalDisks.push_back(kv.second); if (sysInfo.physicalDisks.size()>=8) break; }
    Logger::Debug("物理磁盘枚举完成: " + std::to_string(sysInfo.physicalDisks.size()) + " 个");
}