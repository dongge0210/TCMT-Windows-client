// DiskInfo.h
#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include "../DataStruct/DataStruct.h"

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
    std::vector<DiskData> GetDisks(); // 返回所有磁盘信息

private:
    void QueryDrives();
    std::vector<DriveInfo> drives;
};

