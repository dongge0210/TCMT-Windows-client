// QtDisplayBridge.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <QApplication>
#include <QWidget>
#include <memory>
#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QTreeWidget>

// 系统信息结构体，用于传递系统数据到Qt界面
struct SystemInfo {
    // CPU信息
    std::string cpuName;
    int physicalCores = 0;
    int logicalCores = 0;
    int performanceCores = 0;
    int efficiencyCores = 0;
    double cpuUsage = 0.0;
    bool hyperThreading = false;
    bool virtualization = false;
    double performanceCoreFreq = 0.0;
    double efficiencyCoreFreq = 0.0;

    // 内存信息
    uint64_t totalMemory = 0;
    uint64_t usedMemory = 0;
    uint64_t availableMemory = 0;

    // GPU信息
    std::string gpuName;
    std::string gpuBrand;
    uint64_t gpuMemory = 0;
    double gpuCoreFreq = 0.0;

    // 温度信息（组件名称，温度值）
    std::vector<std::pair<std::string, double>> temperatures;

    // 磁盘信息结构
    struct DiskInfo {
        char letter;
        std::string label;
        std::string fileSystem;
        uint64_t totalSize = 0;
        uint64_t usedSpace = 0;
        uint64_t freeSpace = 0;
    };
    std::vector<DiskInfo> disks;
};

// 前向声明Qt监视窗口类
class SystemMonitorWindow;

/**
 * @brief Qt显示桥接类
 *
 * 这个类作为系统监控核心与Qt GUI之间的桥梁，
 * 负责初始化Qt环境、创建窗口以及更新显示数据
 */
class QtDisplayBridge {
public:
    /**
     * @brief 初始化Qt环境
     *
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * @return bool 初始化是否成功
     */
    static bool Initialize(int argc, char* argv[]);

    /**
     * @brief 创建系统监控窗口
     *
     * @return bool 创建是否成功
     */
    static bool CreateMonitorWindow();

    /**
     * @brief 更新系统信息到Qt界面
     *
     * @param sysInfo 系统信息结构体
     */
    static void UpdateSystemInfo(const SystemInfo& sysInfo);

    /**
     * @brief 检查Qt环境是否已初始化
     *
     * @return bool 是否已初始化
     */
    static bool IsInitialized();

    /**
     * @brief 清理Qt资源
     */
    static void Cleanup();

private:
    static QApplication* qtAppInstance;  // Qt应用程序实例
    static SystemMonitorWindow* monitorWindow;  // 监控窗口实例
    static bool initialized;  // 初始化标志
};
