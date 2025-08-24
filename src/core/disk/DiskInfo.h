// DiskInfo.h
#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <map>
#include "../DataStruct/DataStruct.h"

class WmiManager; // 前向声明，避免头文件依赖膨胀

struct DriveInfo {
    char letter;
    uint64_t totalSize;
    uint64_t freeSpace;
    uint64_t usedSpace;
    std::wstring label;
    std::wstring fileSystem;
};

class DiskInfo {
public:
    DiskInfo(); // 无参数构造
    const std::vector<DriveInfo>& GetDrives() const;
    void Refresh();
    std::vector<DiskData> GetDisks(); // 返回所有逻辑磁盘信息

    // 新增：收集物理磁盘及逻辑盘符映射（不含真正SMART，仅基础+映射）
    static void CollectPhysicalDisks(WmiManager& wmi, const std::vector<DiskData>& logicalDisks, SystemInfo& sysInfo);

private:
    void QueryDrives();
    std::vector<DriveInfo> drives;
};

