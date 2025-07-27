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
    void updateNetworkInfoDisplay(); // 新增：专门更新网络信息显示
    void showSmartDetails(const QString& diskIdentifier);
    void updateNetworkSelector();
    void updateGpuSelector();
    void updateDiskTreeWidget();
    void tryReconnectSharedMemory();  // 添加重新连接函数

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
    
    // Helper method for disk info updates
    void updateDiskInfoFromSharedMemory();
    void updateDiskInfoOptimized(const std::vector<DiskData>& disks); // 新增：优化的磁盘更新方法
    void rebuildDiskUI(const std::vector<DiskData>& disks); // 重建磁盘UI
    void updateDiskDataOnly(const std::vector<DiskData>& disks); // 仅更新数据
    void updateSingleDiskData(const DiskData& disk); // 更新单个磁盘数据

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
    QLabel *networkNameLabel;
    QLabel *networkStatusLabel;
    QLabel *networkTypeLabel;  // 新增：网卡类型标签
    QLabel *networkIpLabel;
    QLabel *networkMacLabel;
    QLabel *networkSpeedLabel;
    QLabel *gpuDriverVersionLabel;

    // Index tracking
    std::vector<int> gpuIndices;
    std::vector<int> networkIndices;
    int currentGpuIndex = 0;
    int currentNetworkIndex = 0;
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

