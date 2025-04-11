// QtDisplayBridge.cpp
#include "QtDisplayBridge.h"
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QGroupBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QMessageBox>
#include "../utils/Logger.h"

// Qt监视器窗口类定义
class SystemMonitorWindow : public QMainWindow {
public:
    SystemMonitorWindow(QWidget* parent = nullptr);
    void updateSystemInfo(const SystemInfo& sysInfo);

private:
    // CPU信息显示组件
    QLabel* cpuNameLabel;
    QLabel* cpuCoresLabel;
    QLabel* cpuThreadsLabel;
    QLabel* cpuPCoresLabel;
    QLabel* cpuECoresLabel;
    QLabel* cpuFeaturesLabel;
    QProgressBar* cpuUsageBar;
    QLabel* cpuPCoreFreqLabel;
    QLabel* cpuECoreFreqLabel;

    // 内存信息显示组件
    QProgressBar* memoryUsageBar;
    QLabel* memoryTotalLabel;
    QLabel* memoryUsedLabel;
    QLabel* memoryAvailableLabel;

    // GPU信息显示组件
    QLabel* gpuNameLabel;
    QLabel* gpuBrandLabel;
    QLabel* gpuMemoryLabel;
    QLabel* gpuFreqLabel;

    // 温度信息显示
    QTreeWidget* temperatureTreeWidget;

    // 磁盘信息显示
    QTreeWidget* diskTreeWidget;

    // 更新界面的方法
    void setupUi();
    void setupCpuInfo(QWidget* cpuTab);
    void setupMemoryInfo(QWidget* memoryTab);
    void setupGpuInfo(QWidget* gpuTab);
    void setupTemperatureInfo(QWidget* tempTab);
    void setupDiskInfo(QWidget* diskTab);
};

// 正确定义静态成员变量
QApplication* QtDisplayBridge::qtAppInstance = nullptr;
SystemMonitorWindow* QtDisplayBridge::monitorWindow = nullptr;
bool QtDisplayBridge::initialized = false;

// SystemMonitorWindow实现
SystemMonitorWindow::SystemMonitorWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("系统硬件监控");
    setMinimumSize(800, 600);
    setupUi();
}

void SystemMonitorWindow::setupUi() {
    // 创建主要布局和选项卡部件
    QTabWidget* tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // 创建各个选项卡
    QWidget* cpuTab = new QWidget();
    QWidget* memoryTab = new QWidget();
    QWidget* gpuTab = new QWidget();
    QWidget* tempTab = new QWidget();
    QWidget* diskTab = new QWidget();

    // 设置各个选项卡的内容
    setupCpuInfo(cpuTab);
    setupMemoryInfo(memoryTab);
    setupGpuInfo(gpuTab);
    setupTemperatureInfo(tempTab);
    setupDiskInfo(diskTab);

    // 添加选项卡到选项卡部件
    tabWidget->addTab(cpuTab, "处理器");
    tabWidget->addTab(memoryTab, "内存");
    tabWidget->addTab(gpuTab, "显卡");
    tabWidget->addTab(tempTab, "温度");
    tabWidget->addTab(diskTab, "磁盘");
}

void SystemMonitorWindow::setupCpuInfo(QWidget* cpuTab) {
    QVBoxLayout* layout = new QVBoxLayout(cpuTab);

    // CPU名称和基本信息
    QGroupBox* infoGroup = new QGroupBox("CPU信息", cpuTab);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);

    cpuNameLabel = new QLabel("名称: 未知");
    cpuCoresLabel = new QLabel("物理核心数: 0");
    cpuThreadsLabel = new QLabel("逻辑线程数: 0");
    cpuPCoresLabel = new QLabel("性能核心数: 0");
    cpuECoresLabel = new QLabel("能效核心数: 0");
    cpuFeaturesLabel = new QLabel("功能支持: 未知");

    infoLayout->addWidget(cpuNameLabel);
    infoLayout->addWidget(cpuCoresLabel);
    infoLayout->addWidget(cpuThreadsLabel);
    infoLayout->addWidget(cpuPCoresLabel);
    infoLayout->addWidget(cpuECoresLabel);
    infoLayout->addWidget(cpuFeaturesLabel);

    // CPU使用率
    QGroupBox* usageGroup = new QGroupBox("CPU使用率", cpuTab);
    QVBoxLayout* usageLayout = new QVBoxLayout(usageGroup);

    cpuUsageBar = new QProgressBar();
    cpuUsageBar->setMinimum(0);
    cpuUsageBar->setMaximum(100);
    cpuUsageBar->setValue(0);
    cpuUsageBar->setFormat("%p%");

    usageLayout->addWidget(cpuUsageBar);

    // CPU频率
    QGroupBox* freqGroup = new QGroupBox("CPU频率", cpuTab);
    QVBoxLayout* freqLayout = new QVBoxLayout(freqGroup);

    cpuPCoreFreqLabel = new QLabel("性能核心频率: 0 GHz");
    cpuECoreFreqLabel = new QLabel("能效核心频率: 0 GHz");

    freqLayout->addWidget(cpuPCoreFreqLabel);
    freqLayout->addWidget(cpuECoreFreqLabel);

    // 添加所有组到主布局
    layout->addWidget(infoGroup);
    layout->addWidget(usageGroup);
    layout->addWidget(freqGroup);
    layout->addStretch(1);
}

void SystemMonitorWindow::setupMemoryInfo(QWidget* memoryTab) {
    QVBoxLayout* layout = new QVBoxLayout(memoryTab);

    // 内存使用率
    QGroupBox* usageGroup = new QGroupBox("内存使用率", memoryTab);
    QVBoxLayout* usageLayout = new QVBoxLayout(usageGroup);

    memoryUsageBar = new QProgressBar();
    memoryUsageBar->setMinimum(0);
    memoryUsageBar->setMaximum(100);
    memoryUsageBar->setValue(0);
    memoryUsageBar->setFormat("%p%");

    usageLayout->addWidget(memoryUsageBar);

    // 内存详细信息
    QGroupBox* detailGroup = new QGroupBox("内存详细信息", memoryTab);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailGroup);

    memoryTotalLabel = new QLabel("总内存: 0 GB");
    memoryUsedLabel = new QLabel("已用内存: 0 GB");
    memoryAvailableLabel = new QLabel("可用内存: 0 GB");

    detailLayout->addWidget(memoryTotalLabel);
    detailLayout->addWidget(memoryUsedLabel);
    detailLayout->addWidget(memoryAvailableLabel);

    // 添加所有组到主布局
    layout->addWidget(usageGroup);
    layout->addWidget(detailGroup);
    layout->addStretch(1);
}

void SystemMonitorWindow::setupGpuInfo(QWidget* gpuTab) {
    QVBoxLayout* layout = new QVBoxLayout(gpuTab);

    // GPU基本信息
    QGroupBox* infoGroup = new QGroupBox("GPU信息", gpuTab);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);

    gpuNameLabel = new QLabel("名称: 未知");
    gpuBrandLabel = new QLabel("品牌: 未知");
    gpuMemoryLabel = new QLabel("显存: 0 GB");
    gpuFreqLabel = new QLabel("频率: 0 MHz");

    infoLayout->addWidget(gpuNameLabel);
    infoLayout->addWidget(gpuBrandLabel);
    infoLayout->addWidget(gpuMemoryLabel);
    infoLayout->addWidget(gpuFreqLabel);

    // 添加所有组到主布局
    layout->addWidget(infoGroup);
    layout->addStretch(1);
}

void SystemMonitorWindow::setupTemperatureInfo(QWidget* tempTab) {
    QVBoxLayout* layout = new QVBoxLayout(tempTab);

    // 温度信息树形控件
    temperatureTreeWidget = new QTreeWidget(tempTab);
    temperatureTreeWidget->setHeaderLabels(QStringList() << "组件" << "温度");
    temperatureTreeWidget->setColumnWidth(0, 250);
    temperatureTreeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    layout->addWidget(temperatureTreeWidget);
}

void SystemMonitorWindow::setupDiskInfo(QWidget* diskTab) {
    QVBoxLayout* layout = new QVBoxLayout(diskTab);

    // 磁盘信息树形控件
    diskTreeWidget = new QTreeWidget(diskTab);
    diskTreeWidget->setHeaderLabels(QStringList() << "驱动器" << "卷标" << "文件系统" << "总容量" << "已用空间" << "可用空间" << "使用率");
    diskTreeWidget->setColumnWidth(0, 80);
    diskTreeWidget->setColumnWidth(1, 150);
    diskTreeWidget->setColumnWidth(2, 80);
    diskTreeWidget->setColumnWidth(3, 100);
    diskTreeWidget->setColumnWidth(4, 100);
    diskTreeWidget->setColumnWidth(5, 100);
    diskTreeWidget->setColumnWidth(6, 80);
    diskTreeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    layout->addWidget(diskTreeWidget);
}

void SystemMonitorWindow::updateSystemInfo(const SystemInfo& sysInfo) {
    // 更新CPU信息
    cpuNameLabel->setText(QString("名称: ") + QString::fromStdString(sysInfo.cpuName));
    cpuCoresLabel->setText(QString("物理核心数: ") + QString::number(sysInfo.physicalCores));
    cpuThreadsLabel->setText(QString("逻辑线程数: ") + QString::number(sysInfo.logicalCores));
    cpuPCoresLabel->setText(QString("性能核心数: ") + QString::number(sysInfo.performanceCores));
    cpuECoresLabel->setText(QString("能效核心数: ") + QString::number(sysInfo.efficiencyCores));

    std::string features = "功能支持: ";
    features += sysInfo.hyperThreading ? "超线程: 是" : "超线程: 否";
    features += ", ";
    features += sysInfo.virtualization ? "虚拟化: 是" : "虚拟化: 否";
    cpuFeaturesLabel->setText(features.c_str());

    cpuUsageBar->setValue(static_cast<int>(sysInfo.cpuUsage));

    char pFreqBuf[32], eFreqBuf[32];
    snprintf(pFreqBuf, sizeof(pFreqBuf), "性能核心频率: %.2f GHz", sysInfo.performanceCoreFreq / 1000.0);
    snprintf(eFreqBuf, sizeof(eFreqBuf), "能效核心频率: %.2f GHz", sysInfo.efficiencyCoreFreq / 1000.0);
    cpuPCoreFreqLabel->setText(pFreqBuf);
    cpuECoreFreqLabel->setText(eFreqBuf);

    // 更新内存信息
    double memUsagePercent = 0;
    if (sysInfo.totalMemory > 0) {
        memUsagePercent = (static_cast<double>(sysInfo.usedMemory) / sysInfo.totalMemory) * 100.0;
    }
    memoryUsageBar->setValue(static_cast<int>(memUsagePercent));

    char memBuf[3][64];
    snprintf(memBuf[0], sizeof(memBuf[0]), "总内存: %.2f GB", sysInfo.totalMemory / (1024.0 * 1024.0 * 1024.0));
    snprintf(memBuf[1], sizeof(memBuf[1]), "已用内存: %.2f GB", sysInfo.usedMemory / (1024.0 * 1024.0 * 1024.0));
    snprintf(memBuf[2], sizeof(memBuf[2]), "可用内存: %.2f GB", sysInfo.availableMemory / (1024.0 * 1024.0 * 1024.0));

    memoryTotalLabel->setText(memBuf[0]);
    memoryUsedLabel->setText(memBuf[1]);
    memoryAvailableLabel->setText(memBuf[2]);

    // 更新GPU信息
    gpuNameLabel->setText(QString("名称: ") + QString::fromStdString(sysInfo.gpuName));
    gpuBrandLabel->setText(QString("品牌: ") + QString::fromStdString(sysInfo.gpuBrand));

    char gpuMemBuf[64], gpuFreqBuf[64];
    snprintf(gpuMemBuf, sizeof(gpuMemBuf), "显存: %.2f GB", sysInfo.gpuMemory / (1024.0 * 1024.0 * 1024.0));
    snprintf(gpuFreqBuf, sizeof(gpuFreqBuf), "频率: %.0f MHz", sysInfo.gpuCoreFreq);

    gpuMemoryLabel->setText(gpuMemBuf);
    gpuFreqLabel->setText(gpuFreqBuf);

    // 更新温度信息
    temperatureTreeWidget->clear();
    for (const auto& temp : sysInfo.temperatures) {
        QTreeWidgetItem* item = new QTreeWidgetItem(temperatureTreeWidget);
        item->setText(0, temp.first.c_str());

        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f °C", temp.second);
        item->setText(1, tempBuf);

        // 为高温设置红色警告
        if (temp.second > 80.0) {
            item->setForeground(1, Qt::red);
        }
        else if (temp.second > 70.0) {
            item->setForeground(1, QColor(255, 165, 0)); // 橙色
        }
    }

    // 更新磁盘信息
    diskTreeWidget->clear();
    for (const auto& disk : sysInfo.disks) {
        QTreeWidgetItem* item = new QTreeWidgetItem(diskTreeWidget);

        char driveBuf[8];
        snprintf(driveBuf, sizeof(driveBuf), "%c:", disk.letter);
        item->setText(0, driveBuf);
        item->setText(1, disk.label.c_str());
        item->setText(2, disk.fileSystem.c_str());

        char sizeBufs[4][32];
        snprintf(sizeBufs[0], sizeof(sizeBufs[0]), "%.2f GB", disk.totalSize / (1024.0 * 1024.0 * 1024.0));
        snprintf(sizeBufs[1], sizeof(sizeBufs[1]), "%.2f GB", disk.usedSpace / (1024.0 * 1024.0 * 1024.0));
        snprintf(sizeBufs[2], sizeof(sizeBufs[2]), "%.2f GB", disk.freeSpace / (1024.0 * 1024.0 * 1024.0));

        double usagePercent = 0;
        if (disk.totalSize > 0) {
            usagePercent = (static_cast<double>(disk.usedSpace) / disk.totalSize) * 100.0;
        }
        snprintf(sizeBufs[3], sizeof(sizeBufs[3]), "%.1f%%", usagePercent);

        item->setText(3, sizeBufs[0]);
        item->setText(4, sizeBufs[1]);
        item->setText(5, sizeBufs[2]);
        item->setText(6, sizeBufs[3]);

        // 为快满的磁盘设置警告色
        if (usagePercent > 90.0) {
            item->setForeground(6, Qt::red);
        }
        else if (usagePercent > 75.0) {
            item->setForeground(6, QColor(255, 165, 0)); // 橙色
        }
    }
}

// QtDisplayBridge实现
bool QtDisplayBridge::Initialize(int argc, char* argv[]) {
    try {
        if (initialized) {
            Logger::Warning("Qt环境已经初始化");
            return true;
        }

        // 创建Qt应用程序实例
        qtAppInstance = new QApplication(argc, argv);
        if (!qtAppInstance) {
            Logger::Error("无法创建Qt应用程序实例");
            return false;
        }

        // 设置应用程序基本信息
        QApplication::setApplicationName("系统硬件监控");
        QApplication::setOrganizationName("硬件监控工具");

        initialized = true;
        Logger::Info("Qt环境初始化成功");
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("Qt环境初始化失败: " + std::string(e.what()));
        return false;
    }
}

bool QtDisplayBridge::CreateMonitorWindow() {
    try {
        if (!initialized) {
            Logger::Error("尝试创建窗口前请先初始化Qt环境");
            return false;
        }

        if (monitorWindow) {
            Logger::Warning("监控窗口已经创建");
            return true;
        }

        // 创建和显示监控窗口
        monitorWindow = new SystemMonitorWindow();
        monitorWindow->show();

        Logger::Info("监控窗口创建成功");
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("监控窗口创建失败: " + std::string(e.what()));
        return false;
    }
}

void QtDisplayBridge::UpdateSystemInfo(const SystemInfo& sysInfo) {
    if (!initialized || !monitorWindow) {
        return;
    }

    // 通过Qt事件循环安全地更新UI
    auto* win = monitorWindow; // capture a local pointer for the lambda
    QMetaObject::invokeMethod(win, [sysInfo, win]() {
        win->updateSystemInfo(sysInfo);
    }, Qt::QueuedConnection);
}

bool QtDisplayBridge::IsInitialized() {
    return initialized;
}

void QtDisplayBridge::Cleanup() {
    // 清理窗口和应用程序实例
    if (monitorWindow) {
        monitorWindow->close();
        delete monitorWindow;
        monitorWindow = nullptr;
    }

    if (qtAppInstance) {
        // 不需要显式删除qtAppInstance，它会在应用程序结束时自动清理
        qtAppInstance = nullptr;
    }

    initialized = false;
    Logger::Info("Qt资源已清理");
}
