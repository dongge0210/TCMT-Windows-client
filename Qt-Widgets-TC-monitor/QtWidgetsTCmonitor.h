// QtWidgetsTCmonitor.h
#pragma once

#include "stdafx.h"
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtGui/QPainter>
#include <QTimer>
#include <queue>
#include <map>
#include <sstream>
#include <iomanip>
#include "../src/core/DataStruct/DataStruct.h" // Include header for SystemInfo
#include "../src/core/utils/Logger.h"

// 前向声明
struct PhysicalDiskSmartData;

// QT-UI uses SharedMemoryManager::GetBuffer() for data access

// Define the maximum number of data points for charts
constexpr int MAX_DATA_POINTS = 60;

class QtWidgetsTCmonitor : public QMainWindow
{
    Q_OBJECT

public:
    QtWidgetsTCmonitor(QWidget *parent = nullptr);
    ~QtWidgetsTCmonitor();

    // Updates system information display with provided data
    void updateSystemInfo(const SystemInfo& sysInfo);
    void updateTemperatureData(const std::vector<std::pair<std::string, float>>& temperatures);

private slots:
    void updateCharts();
    void on_pushButton_clicked();
    void updateFromSharedMemory();
    void onGpuSelectionChanged(int index);
    void onNetworkSelectionChanged(int index);
    void onDiskSelectionChanged(int index);  // 新增：磁盘选择变化处理
    void updateNetworkInfoDisplay(); // 网络信息显示更新
    void showSmartDetails(const QString& diskIdentifier);
    void showCurrentDiskSmartDetails(); // 新增：显示当前选中磁盘的SMART详情
    void updateNetworkSelector();
    void updateGpuSelector();
    void updateDiskSelector();  // 新增：更新磁盘选择器
    void updateDiskTreeWidget();
    void tryReconnectSharedMemory();  // 重新连接函数

private:
    // UI setup functions
    void setupUI();
    void createCpuSection();
    void createMemorySection();
    void createGpuSection();
    void createTemperatureSection();
    void createNetworkSection();
    void createDiskSection();

    // Formatting helper functions
    QString formatSize(uint64_t bytes);
    QString formatPercentage(double value);
    QString formatTemperature(double value);
    QString formatFrequency(double value);
    QString formatNetworkSpeed(uint64_t speedBps);
    
    // Date/time update function
    void updateLocalDateTime();

    // Safe string conversion helper
    QString safeFromWCharArray(const wchar_t* wstr, size_t maxLen);
    
    // 物理磁盘信息更新函数
    void updateDiskInfoDisplay(); // 新增：更新磁盘信息显示
    void resetDiskInfoLabels(); // 新增：重置磁盘信息标签
    
    // SMART功能支持函数
    void createDemoSmartData(PhysicalDiskSmartData& smartData, const QString& diskIdentifier);
    
    // 保持兼容性的旧函数（标记为废弃）
    void updateDiskInfoFromSharedMemory();
    void updateDiskInfoOptimized(const std::vector<DiskData>& disks);
    void rebuildDiskUI(const std::vector<DiskData>& disks);
    void updateDiskDataOnly(const std::vector<DiskData>& disks);
    void updateSingleDiskData(const DiskData& disk);

    // UI components
    QTimer *updateTimer;

    // Main UI containers
    QGroupBox *cpuGroupBox;
    QGroupBox *memoryGroupBox;
    QGroupBox *gpuGroupBox;
    QGroupBox *temperatureGroupBox;
    QGroupBox *diskGroupBox;
    QGroupBox *networkGroupBox;

    // Network section components
    QComboBox *gpuSelector;
    QComboBox *networkSelector;
    QComboBox *diskSelector;  // 新增：磁盘选择器
    QLabel *networkNameLabel;
    QLabel *networkStatusLabel;
    QLabel *networkTypeLabel;  // 网卡类型标签
    QLabel *networkIpLabel;
    QLabel *networkMacLabel;
    QLabel *networkSpeedLabel;
    QLabel *gpuDriverVersionLabel;

    // 逻辑驱动器显示布局
    QVBoxLayout *logicalDrivesLayout;  // 新增：逻辑驱动器布局

    // Index tracking
    std::vector<int> gpuIndices;
    std::vector<int> networkIndices;
    std::vector<int> diskIndices;  // 新增：磁盘索引
    int currentGpuIndex = 0;
    int currentNetworkIndex = 0;
    int currentDiskIndex = 0;  // 新增：当前磁盘索引
    int cachedGpuCount = -1;

    // UI reference
    class Ui_QtWidgetsTCmonitorClass *ui;

    // Chart components
    QChart *cpuTempChart;
    QChartView *cpuTempChartView;
    QLineSeries *cpuTempSeries;
    QChart *gpuTempChart;
    QChartView *gpuTempChartView;
    QLineSeries *gpuTempSeries;

    // Data containers
    std::map<std::string, QLabel*> infoLabels;
    std::queue<float> cpuTempHistory;
    std::queue<float> gpuTempHistory;
    SystemInfo currentSysInfo;
    
    // 磁盘信息缓存 - 避免频繁重建UI
    std::vector<QGroupBox*> diskBoxes;
    std::map<char, std::map<std::string, QLabel*>> diskLabels; // 磁盘盘符 -> 标签类型 -> QLabel指针
};

