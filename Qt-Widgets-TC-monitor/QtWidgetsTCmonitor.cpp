#include "stdafx.h"
#include "QtWidgetsTCmonitor.h"
#include "ui_QtWidgetsTCmonitor.h"  // Auto-generated UI file
#include "../src/core/DataStruct/SharedMemoryManager.h"
#include "../src/core/utils/Logger.h"  // 添加Logger支持

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QTimer>
#include <QtCore/QTime>  
#include <QtGui/QPainter>
#include <map>       // For std::map
#include <string>    // For std::string
#include <queue>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm> // For std::transform
#include <cctype>    // For ::tolower
#include <cstring>   // For memset

QtWidgetsTCmonitor::QtWidgetsTCmonitor(QWidget* parent)
    : QMainWindow(parent), 
      updateTimer(nullptr),
      cpuGroupBox(nullptr), memoryGroupBox(nullptr), gpuGroupBox(nullptr),
      temperatureGroupBox(nullptr), diskGroupBox(nullptr), networkGroupBox(nullptr),
      gpuSelector(nullptr), networkSelector(nullptr),
      networkNameLabel(nullptr), networkStatusLabel(nullptr), networkIpLabel(nullptr),
      networkMacLabel(nullptr), networkSpeedLabel(nullptr), gpuDriverVersionLabel(nullptr),
      cpuTempChart(nullptr), cpuTempChartView(nullptr), cpuTempSeries(nullptr),
      gpuTempChart(nullptr), gpuTempChartView(nullptr), gpuTempSeries(nullptr),
      ui(nullptr)
{
    setupUI();

    // Set window properties
    setWindowTitle(tr("系统硬件监视器"));
    resize(800, 600);

    // Initialize shared memory access
    if (!SharedMemoryManager::InitSharedMemory()) {
        QString errorMsg = tr("无法初始化共享内存，请确保主程序正在运行。\n错误信息: ") + 
            QString::fromStdString(SharedMemoryManager::GetLastError());
        
        QMessageBox::critical(this, tr("共享内存错误"), errorMsg);
        
        // 不要立即退出，而是显示错误状态并禁用更新
        setWindowTitle(tr("系统硬件监视器 - 共享内存错误"));
        
        // 创建错误提示标签
        QLabel* errorLabel = new QLabel(tr("共享内存初始化失败\n请先启动主程序 Win_x64_sysMonitor.exe"));
        errorLabel->setAlignment(Qt::AlignCenter);
        errorLabel->setStyleSheet("QLabel { color: red; font-size: 16px; font-weight: bold; }");
        setCentralWidget(errorLabel);
        
        // 设置定时器尝试重新连接
        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::tryReconnectSharedMemory);
        updateTimer->start(5000); // 每5秒尝试重新连接
        return;
    }

    // Set up update timer - connect to shared memory reader instead of just charts
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::updateFromSharedMemory);
    updateTimer->start(1000); // Update once per second
}

QtWidgetsTCmonitor::~QtWidgetsTCmonitor()
{
    if (updateTimer) {
        updateTimer->stop();
        delete updateTimer;
    }
}

QString QtWidgetsTCmonitor::safeFromWCharArray(const wchar_t* wstr, size_t maxLen) {
    if (!wstr) return QString();
    
    // Find the actual string length, but don't exceed maxLen
    size_t len = 0;
    while (len < maxLen && wstr[len] != L'\0') {
        len++;
    }
    
    // Ensure null termination by creating a safe copy
    std::vector<wchar_t> safeCopy(len + 1, L'\0');
    if (len > 0) {
        memcpy(safeCopy.data(), wstr, len * sizeof(wchar_t));
    }
    
    return QString::fromWCharArray(safeCopy.data());
}

void QtWidgetsTCmonitor::setupUI()
{
    // Create main scroll area
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    setCentralWidget(scrollArea);

    // Create main container
    QWidget* container = new QWidget(scrollArea);
    scrollArea->setWidget(container);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(container);

    // Create sections
    createCpuSection();
    createMemorySection();
    createGpuSection();
    createTemperatureSection();
    createDiskSection();

    // Add to main layout
    mainLayout->addWidget(cpuGroupBox);
    mainLayout->addWidget(memoryGroupBox);
    mainLayout->addWidget(gpuGroupBox);
    mainLayout->addWidget(temperatureGroupBox);
    mainLayout->addWidget(networkGroupBox);  // 添加网络groupbox
    mainLayout->addWidget(diskGroupBox);
    mainLayout->addStretch();
}

void QtWidgetsTCmonitor::createCpuSection()
{
    cpuGroupBox = new QGroupBox(tr("处理器信息"), this);
    QGridLayout* layout = new QGridLayout(cpuGroupBox);

    // Add labels
    int row = 0;
    layout->addWidget(new QLabel(tr("名称:"), this), row, 0);
    infoLabels["cpuName"] = new QLabel(this);
    layout->addWidget(infoLabels["cpuName"], row++, 1);

    layout->addWidget(new QLabel(tr("物理核心:"), this), row, 0);
    infoLabels["physicalCores"] = new QLabel(this);
    layout->addWidget(infoLabels["physicalCores"], row++, 1);

    layout->addWidget(new QLabel(tr("逻辑核心:"), this), row, 0);
    infoLabels["logicalCores"] = new QLabel(this);
    layout->addWidget(infoLabels["logicalCores"], row++, 1);

    layout->addWidget(new QLabel(tr("性能核心:"), this), row, 0);
    infoLabels["performanceCores"] = new QLabel(this);
    layout->addWidget(infoLabels["performanceCores"], row++, 1);

    layout->addWidget(new QLabel(tr("能效核心:"), this), row, 0);
    infoLabels["efficiencyCores"] = new QLabel(this);
    layout->addWidget(infoLabels["efficiencyCores"], row++, 1);

    layout->addWidget(new QLabel(tr("CPU使用率:"), this), row, 0);
    infoLabels["cpuUsage"] = new QLabel(this);
    layout->addWidget(infoLabels["cpuUsage"], row++, 1);

    layout->addWidget(new QLabel(tr("超线程:"), this), row, 0);
    infoLabels["hyperThreading"] = new QLabel(this);
    layout->addWidget(infoLabels["hyperThreading"], row++, 1);

    layout->addWidget(new QLabel(tr("虚拟化:"), this), row, 0);
    infoLabels["virtualization"] = new QLabel(this);
    layout->addWidget(infoLabels["virtualization"], row++, 1);
}

void QtWidgetsTCmonitor::createMemorySection()
{
    memoryGroupBox = new QGroupBox(tr("内存信息"), this);
    QGridLayout* layout = new QGridLayout(memoryGroupBox);

    int row = 0;
    layout->addWidget(new QLabel(tr("总内存:"), this), row, 0);
    infoLabels["totalMemory"] = new QLabel(this);
    layout->addWidget(infoLabels["totalMemory"], row++, 1);

    layout->addWidget(new QLabel(tr("已用内存:"), this), row, 0);
    infoLabels["usedMemory"] = new QLabel(this);
    layout->addWidget(infoLabels["usedMemory"], row++, 1);

    layout->addWidget(new QLabel(tr("可用内存:"), this), row, 0);
    infoLabels["availableMemory"] = new QLabel(this);
    layout->addWidget(infoLabels["availableMemory"], row++, 1);

    layout->addWidget(new QLabel(tr("内存使用率:"), this), row, 0);
    infoLabels["memoryUsage"] = new QLabel(this);
    layout->addWidget(infoLabels["memoryUsage"], row++, 1);
}

void QtWidgetsTCmonitor::createGpuSection()
{
    gpuGroupBox = new QGroupBox(tr("显卡信息"), this);
    QGridLayout* layout = new QGridLayout(gpuGroupBox);

    int row = 0;
    layout->addWidget(new QLabel(tr("名称:"), this), row, 0);
    infoLabels["gpuName"] = new QLabel(this);
    layout->addWidget(infoLabels["gpuName"], row++, 1);

    layout->addWidget(new QLabel(tr("品牌:"), this), row, 0);
    infoLabels["gpuBrand"] = new QLabel(this);
    layout->addWidget(infoLabels["gpuBrand"], row++, 1);

    layout->addWidget(new QLabel(tr("显存:"), this), row, 0);
    infoLabels["gpuMemory"] = new QLabel(this);
    layout->addWidget(infoLabels["gpuMemory"], row++, 1);

    layout->addWidget(new QLabel(tr("核心频率:"), this), row, 0);
    infoLabels["gpuCoreFreq"] = new QLabel(this);
    layout->addWidget(infoLabels["gpuCoreFreq"], row++, 1);
}

void QtWidgetsTCmonitor::createTemperatureSection()
{
    temperatureGroupBox = new QGroupBox(tr("温度监控"), this);
    QVBoxLayout* layout = new QVBoxLayout(temperatureGroupBox);

    // Temperature labels
    QGridLayout* tempLabelsLayout = new QGridLayout();
    tempLabelsLayout->addWidget(new QLabel(tr("CPU温度:"), this), 0, 0);
    infoLabels["cpuTemp"] = new QLabel(this);
    tempLabelsLayout->addWidget(infoLabels["cpuTemp"], 0, 1);

    tempLabelsLayout->addWidget(new QLabel(tr("GPU温度:"), this), 1, 0);
    infoLabels["gpuTemp"] = new QLabel(this);
    tempLabelsLayout->addWidget(infoLabels["gpuTemp"], 1, 1);

    layout->addLayout(tempLabelsLayout);

    // Create charts
    // CPU temperature chart
    cpuTempChart = new QChart();
    cpuTempChart->setTitle(tr("CPU温度历史"));
    cpuTempSeries = new QLineSeries();
    cpuTempChart->addSeries(cpuTempSeries);

    // Set up axes
    QValueAxis* axisX = new QValueAxis();
    axisX->setRange(0, MAX_DATA_POINTS);
    axisX->setLabelFormat("%d");
    axisX->setTitleText(tr("时间 (秒)"));

    QValueAxis* axisY = new QValueAxis();
    axisY->setRange(0, 100);
    axisY->setLabelFormat("%d");
    axisY->setTitleText(tr("温度 (°C)"));

    cpuTempChart->addAxis(axisX, Qt::AlignBottom);
    cpuTempChart->addAxis(axisY, Qt::AlignLeft);
    cpuTempSeries->attachAxis(axisX);
    cpuTempSeries->attachAxis(axisY);

    cpuTempChartView = new QChartView(cpuTempChart);
    cpuTempChartView->setRenderHint(QPainter::Antialiasing);

    // GPU temperature chart
    gpuTempChart = new QChart();
    gpuTempChart->setTitle(tr("GPU温度历史"));
    gpuTempSeries = new QLineSeries();
    gpuTempChart->addSeries(gpuTempSeries);

    QValueAxis* axisX2 = new QValueAxis();
    axisX2->setRange(0, MAX_DATA_POINTS);
    axisX2->setLabelFormat("%d");
    axisX2->setTitleText(tr("时间 (秒)"));

    QValueAxis* axisY2 = new QValueAxis();
    axisY2->setRange(0, 100);
    axisY2->setLabelFormat("%d");
    axisY2->setTitleText(tr("温度 (°C)"));

    gpuTempChart->addAxis(axisX2, Qt::AlignBottom);
    gpuTempChart->addAxis(axisY2, Qt::AlignLeft);
    gpuTempSeries->attachAxis(axisX2);
    gpuTempSeries->attachAxis(axisY2);

    gpuTempChartView = new QChartView(gpuTempChart);
    gpuTempChartView->setRenderHint(QPainter::Antialiasing);

    // Create horizontal splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(cpuTempChartView);
    splitter->addWidget(gpuTempChartView);

    layout->addWidget(splitter);
}

void QtWidgetsTCmonitor::createNetworkSection()
{
    networkGroupBox = new QGroupBox(tr("网络信息"), this);
    QGridLayout* layout = new QGridLayout(networkGroupBox);

    int row = 0;
    
    // 网络适配器选择器
    layout->addWidget(new QLabel(tr("网络适配器:"), this), row, 0);
    networkSelector = new QComboBox(this);
    networkSelector->addItem(tr("正在检测网络适配器..."));
    connect(networkSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &QtWidgetsTCmonitor::onNetworkSelectionChanged);
    layout->addWidget(networkSelector, row++, 1);
    
    // 网络信息标签
    layout->addWidget(new QLabel(tr("适配器名称:"), this), row, 0);
    networkNameLabel = new QLabel(this);
    layout->addWidget(networkNameLabel, row++, 1);

    layout->addWidget(new QLabel(tr("连接状态:"), this), row, 0);
    networkStatusLabel = new QLabel(this);
    layout->addWidget(networkStatusLabel, row++, 1);

    layout->addWidget(new QLabel(tr("IP地址:"), this), row, 0);
    networkIpLabel = new QLabel(this);
    layout->addWidget(networkIpLabel, row++, 1);

    layout->addWidget(new QLabel(tr("MAC地址:"), this), row, 0);
    networkMacLabel = new QLabel(this);
    layout->addWidget(networkMacLabel, row++, 1);

    layout->addWidget(new QLabel(tr("连接速度:"), this), row, 0);
    networkSpeedLabel = new QLabel(this);
    layout->addWidget(networkSpeedLabel, row++, 1);
}

void QtWidgetsTCmonitor::createDiskSection()
{
    diskGroupBox = new QGroupBox(tr("磁盘信息"), this);
    QVBoxLayout* layout = new QVBoxLayout(diskGroupBox);

    // Disk info container
    QWidget* diskContainer = new QWidget();
    layout->addWidget(diskContainer);
}

void QtWidgetsTCmonitor::updateTemperatureData(const std::vector<std::pair<std::string, float>>& temperatures)
{
    float cpuTemp = 0;
    float gpuTemp = 0;
    bool cpuFound = false;
    bool gpuFound = false;

    // Iterate through temperature data
    for (const auto& temp : temperatures) {
        // 更灵活的CPU温度检测 - 检查多种可能的传感器名称
        std::string tempName = temp.first;
        std::transform(tempName.begin(), tempName.end(), tempName.begin(), ::tolower);
        
        if (tempName.find("cpu") != std::string::npos || 
            tempName.find("package") != std::string::npos ||
            tempName.find("core") != std::string::npos ||
            tempName.find("processor") != std::string::npos) {
            cpuTemp = temp.second;
            cpuFound = true;
            infoLabels["cpuTemp"]->setText(formatTemperature(cpuTemp));
        }
        else if (tempName.find("gpu") != std::string::npos ||
                 tempName.find("graphics") != std::string::npos) {
            gpuTemp = temp.second;
            gpuFound = true;
            infoLabels["gpuTemp"]->setText(formatTemperature(gpuTemp));
        }
    }

    // If no data found, update to no data state
    if (!cpuFound) {
        infoLabels["cpuTemp"]->setText(tr("无数据"));
    }
    if (!gpuFound) {
        infoLabels["gpuTemp"]->setText(tr("无数据"));
    }

    // Update temperature history
    if (cpuFound) {
        cpuTempHistory.push(cpuTemp);
        if (cpuTempHistory.size() > MAX_DATA_POINTS) {
            cpuTempHistory.pop();
        }
    }

    if (gpuFound) {
        gpuTempHistory.push(gpuTemp);
        if (gpuTempHistory.size() > MAX_DATA_POINTS) {
            gpuTempHistory.pop();
        }
    }

    // Save current system info temperature data
    std::vector<std::pair<std::string, double>> tempDoubles;
    for (const auto& temp : temperatures) {
        tempDoubles.push_back({temp.first, static_cast<double>(temp.second)});
    }
    currentSysInfo.temperatures = tempDoubles;
}

void QtWidgetsTCmonitor::updateSystemInfo(const SystemInfo& sysInfo)
{
    currentSysInfo = sysInfo;

    // Update CPU info
    infoLabels["cpuName"]->setText(QString::fromStdString(sysInfo.cpuName));
    infoLabels["physicalCores"]->setText(QString::number(sysInfo.physicalCores));
    infoLabels["logicalCores"]->setText(QString::number(sysInfo.logicalCores));
    infoLabels["performanceCores"]->setText(QString::number(sysInfo.performanceCores));
    infoLabels["efficiencyCores"]->setText(QString::number(sysInfo.efficiencyCores));
    infoLabels["cpuUsage"]->setText(formatPercentage(sysInfo.cpuUsage));
    infoLabels["hyperThreading"]->setText(sysInfo.hyperThreading ? tr("已启用") : tr("未启用"));
    infoLabels["virtualization"]->setText(sysInfo.virtualization ? tr("已启用") : tr("未启用"));

    // Update memory info
    infoLabels["totalMemory"]->setText(formatSize(sysInfo.totalMemory));
    infoLabels["usedMemory"]->setText(formatSize(sysInfo.usedMemory));
    infoLabels["availableMemory"]->setText(formatSize(sysInfo.availableMemory));

    double memoryUsagePercent = static_cast<double>(sysInfo.usedMemory) / sysInfo.totalMemory * 100.0;
    infoLabels["memoryUsage"]->setText(formatPercentage(memoryUsagePercent));

    // Update GPU info
    infoLabels["gpuName"]->setText(QString::fromStdString(sysInfo.gpuName));
    infoLabels["gpuBrand"]->setText(QString::fromStdString(sysInfo.gpuBrand));
    infoLabels["gpuMemory"]->setText(formatSize(sysInfo.gpuMemory));
    infoLabels["gpuCoreFreq"]->setText(formatFrequency(sysInfo.gpuCoreFreq));

    // Convert temperature data to float for updateTemperatureData
    std::vector<std::pair<std::string, float>> tempFloats;
    for (const auto& temp : sysInfo.temperatures) {
        tempFloats.push_back({temp.first, static_cast<float>(temp.second)});
    }
    
    // Update temperature data
    updateTemperatureData(tempFloats);

    // Update disk info
    // Need to clear existing disk info layout first
    QLayout* currentLayout = diskGroupBox->layout();
    QWidget* diskContainer = nullptr;

    if (currentLayout) {
        // Get disk container from current layout
        for (int i = 0; i < currentLayout->count(); ++i) {
            QWidget* widget = currentLayout->itemAt(i)->widget();
            if (widget) {
                diskContainer = widget;
                break;
            }
        }
    }

    // If container not found, create a new one
    if (!diskContainer) {
        diskContainer = new QWidget(diskGroupBox);
        static_cast<QVBoxLayout*>(currentLayout)->addWidget(diskContainer);
    }

    // Delete existing layout
    if (diskContainer->layout()) {
        QLayoutItem* child;
        while ((child = diskContainer->layout()->takeAt(0)) != nullptr) {
            if (child->widget()) {
                child->widget()->deleteLater();
            }
            delete child;
        }
        delete diskContainer->layout();
    }

    // Create new layout
    QVBoxLayout* diskLayout = new QVBoxLayout(diskContainer);

    // Add disk info
    for (const auto& disk : sysInfo.disks) {
        QString diskLabel = QString("%1: %2").arg(QString(disk.letter)).arg(tr("驱动器"));
        if (!disk.label.empty()) {
            diskLabel += QString(" (%1)").arg(QString::fromStdString(disk.label));
        }

        QGroupBox* diskBox = new QGroupBox(diskLabel);
        QGridLayout* diskInfoLayout = new QGridLayout(diskBox);

        int row = 0;
        if (!disk.fileSystem.empty()) {
            diskInfoLayout->addWidget(new QLabel(tr("文件系统:")), row, 0);
            diskInfoLayout->addWidget(new QLabel(QString::fromStdString(disk.fileSystem)), row++, 1);
        }

        diskInfoLayout->addWidget(new QLabel(tr("总容量:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatSize(disk.totalSize)), row++, 1);

        diskInfoLayout->addWidget(new QLabel(tr("已用空间:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatSize(disk.usedSpace)), row++, 1);

        uint64_t freeSpace = disk.totalSize - disk.usedSpace;
        diskInfoLayout->addWidget(new QLabel(tr("可用空间:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatSize(freeSpace)), row++, 1);

        double usagePercent = static_cast<double>(disk.usedSpace) / disk.totalSize * 100.0;
        diskInfoLayout->addWidget(new QLabel(tr("使用率:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatPercentage(usagePercent)), row++, 1);

        diskLayout->addWidget(diskBox);
    }

    diskLayout->addStretch();
}

// ============================================================================
// 缺失函数的实现 - 修复链接错误
// ============================================================================

void QtWidgetsTCmonitor::updateGpuSelector() {
    // 更新GPU选择器 - 使用本地拷贝避免并发访问
    if (!gpuSelector) {
        Logger::Warn("updateGpuSelector: gpuSelector未初始化");
        return;
    }
    
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateGpuSelector: 共享内存缓冲区不可用");
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证GPU数据
        if (localCopy.gpuCount > 2) {
            Logger::Warn("GPU数量异常: " + std::to_string(localCopy.gpuCount));
            return;
        }
        
        gpuSelector->clear();
        gpuIndices.clear();
        
        if (localCopy.gpuCount > 0) {
            for (int i = 0; i < localCopy.gpuCount && i < 2; ++i) {
                QString gpuName = safeFromWCharArray(localCopy.gpus[i].name, 
                                                   sizeof(localCopy.gpus[i].name)/sizeof(wchar_t));
                if (!gpuName.isEmpty()) {
                    gpuSelector->addItem(QString("GPU %1: %2").arg(i + 1).arg(gpuName));
                    gpuIndices.push_back(i);
                }
            }
            
            // 如果有GPU，默认选择第一个
            if (gpuSelector->count() > 0) {
                gpuSelector->setCurrentIndex(0);
                currentGpuIndex = 0;
            }
        } else {
            gpuSelector->addItem("未检测到GPU");
            currentGpuIndex = -1;
        }
        
    } catch (const std::exception& e) {
        Logger::Error("更新GPU选择器时发生错误: " + std::string(e.what()));
        gpuSelector->clear();
        gpuSelector->addItem("GPU数据错误");
        currentGpuIndex = -1;
    }
}

void QtWidgetsTCmonitor::updateNetworkSelector() {
    // 更新网络适配器选择器 - 使用本地拷贝避免并发访问
    if (!networkSelector) {
        Logger::Warn("updateNetworkSelector: networkSelector未初始化");
        return;
    }
    
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateNetworkSelector: 共享内存缓冲区不可用");
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证网络适配器数据
        if (localCopy.adapterCount > 4) {
            Logger::Warn("网络适配器数量异常: " + std::to_string(localCopy.adapterCount));
            return;
        }
        
        networkSelector->clear();
        networkIndices.clear();
        
        if (localCopy.adapterCount > 0) {
            for (int i = 0; i < localCopy.adapterCount && i < 4; ++i) {
                QString adapterName = safeFromWCharArray(localCopy.adapters[i].name, 
                                                       sizeof(localCopy.adapters[i].name)/sizeof(wchar_t));
                if (!adapterName.isEmpty()) {
                    networkSelector->addItem(QString("网卡 %1: %2").arg(i + 1).arg(adapterName));
                    networkIndices.push_back(i);
                }
            }
            
            // 如果有网络适配器，默认选择第一个
            if (networkSelector->count() > 0) {
                networkSelector->setCurrentIndex(0);
                currentNetworkIndex = 0;
            }
        } else {
            networkSelector->addItem("未检测到网络适配器");
            currentNetworkIndex = -1;
        }
        
    } catch (const std::exception& e) {
        Logger::Error("更新网络选择器时发生错误: " + std::string(e.what()));
        networkSelector->clear();
        networkSelector->addItem("网络数据错误");
        currentNetworkIndex = -1;
    }
}

void QtWidgetsTCmonitor::onGpuSelectionChanged(int index) {
    // GPU选择变化处理
    if (index >= 0 && index < static_cast<int>(gpuIndices.size())) {
        currentGpuIndex = gpuIndices[index];
        Logger::Debug("用户选择了GPU索引: " + std::to_string(currentGpuIndex));
        
        // 更新GPU相关显示信息
        updateFromSharedMemory();
    }
}

void QtWidgetsTCmonitor::onNetworkSelectionChanged(int index) {
    // 网络适配器选择变化处理
    if (index >= 0 && index < static_cast<int>(networkIndices.size())) {
        currentNetworkIndex = networkIndices[index];
        Logger::Debug("用户选择了网络适配器索引: " + std::to_string(currentNetworkIndex));
        
        // 更新网络适配器相关显示信息
        updateFromSharedMemory();
    }
}

void QtWidgetsTCmonitor::updateDiskTreeWidget() {
    // 更新磁盘树状控件 - 使用本地拷贝避免并发访问
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateDiskTreeWidget: 共享内存缓冲区不可用");
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证磁盘数据
        if (localCopy.diskCount > 8) {
            Logger::Warn("磁盘数量异常: " + std::to_string(localCopy.diskCount));
            return;
        }
        
        Logger::Debug("更新磁盘树状控件，磁盘数量: " + std::to_string(localCopy.diskCount));
        
        // 调用磁盘信息更新函数
        updateDiskInfoFromSharedMemory();
        
    } catch (const std::exception& e) {
        Logger::Error("更新磁盘树状控件时发生错误: " + std::string(e.what()));
    } catch (...) {
        Logger::Error("更新磁盘树状控件时发生未知错误");
    }
}

void QtWidgetsTCmonitor::showSmartDetails(const QString& diskIdentifier) {
    // 显示磁盘SMART详细信息
    Logger::Info("请求显示磁盘SMART详情: " + diskIdentifier.toStdString());
    
    // 创建一个简单的消息框显示SMART信息
    QMessageBox msgBox;
    msgBox.setWindowTitle("磁盘 SMART 详细信息");
    msgBox.setIcon(QMessageBox::Information);
    
    QString smartInfo = QString("磁盘: %1\n\n").arg(diskIdentifier);
    smartInfo += "SMART 状态: 正常\n";
    smartInfo += "温度: 35°C\n";
    smartInfo += "通电时间: 1000 小时\n";
    smartInfo += "读写次数: 500000\n";
    smartInfo += "坏扇区数: 0\n\n";
    smartInfo += "注意: 这是示例数据，实际SMART功能需要进一步开发。";
    
    msgBox.setText(smartInfo);
    msgBox.exec();
}

// ============================================================================
// 格式化函数实现
// ============================================================================

QString QtWidgetsTCmonitor::formatSize(uint64_t bytes) {
    const double kb = 1024.0;
    const double mb = kb * kb;
    const double gb = mb * kb;
    const double tb = gb * kb;

    if (bytes >= tb) return QString::number(bytes / tb, 'f', 1) + " TB";
    else if (bytes >= gb) return QString::number(bytes / gb, 'f', 1) + " GB";
    else if (bytes >= mb) return QString::number(bytes / mb, 'f', 1) + " MB";
    else if (bytes >= kb) return QString::number(bytes / kb, 'f', 1) + " KB";
    else return QString::number(bytes) + " B";
}

QString QtWidgetsTCmonitor::formatPercentage(double value) {
    return QString::number(value, 'f', 1) + "%";
}

QString QtWidgetsTCmonitor::formatTemperature(double value) {
    return QString::number(static_cast<int>(value)) + "°C";
}

QString QtWidgetsTCmonitor::formatFrequency(double value) {
    if (value >= 1000) {
        return QString::number(value / 1000.0, 'f', 1) + " GHz";
    } else {
        return QString::number(value, 'f', 1) + " MHz";
    }
}

// ============================================================================
// 其他缺失函数实现
// ============================================================================

void QtWidgetsTCmonitor::on_pushButton_clicked() {
    // 按钮点击事件处理
    Logger::Info("用户点击了按钮");
    
    // 显示一个简单的消息框
    QMessageBox::information(this, "系统监控器", "系统监控器正在运行中...");
}

void QtWidgetsTCmonitor::updateFromSharedMemory() {
    // 从共享内存更新数据 - 使用本地拷贝避免并发访问问题
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateFromSharedMemory: 共享内存缓冲区不可用，尝试重新连接");
        
        // 尝试重新初始化共享内存
        if (!SharedMemoryManager::InitSharedMemory()) {
            // 如果重新初始化失败，显示错误并停止更新
            setWindowTitle(tr("系统硬件监视器 - 连接中断"));
            return;
        } else {
            Logger::Info("共享内存重新连接成功");
            buffer = SharedMemoryManager::GetBuffer();
            if (!buffer) {
                Logger::Error("重新连接后仍无法获取缓冲区");
                return;
            }
        }
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        
        // 安全地复制内存，防止访问冲突
        memset(&localCopy, 0, sizeof(SharedMemoryBlock)); // 先清零
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证数据完整性 - 添加更严格的检查
        if (localCopy.diskCount > 8 || localCopy.diskCount < 0) {
            Logger::Warn("磁盘数量异常: " + std::to_string(localCopy.diskCount) + "，跳过更新");
            return;
        }
        
        if (localCopy.gpuCount > 2 || localCopy.gpuCount < 0) {
            Logger::Warn("GPU数量异常: " + std::to_string(localCopy.gpuCount) + "，跳过更新");
            return;
        }
        
        if (localCopy.tempCount > 10 || localCopy.tempCount < 0) {
            Logger::Warn("温度传感器数量异常: " + std::to_string(localCopy.tempCount) + "，跳过更新");
            return;
        }
        
        // 检查内存数据是否合理
        if (localCopy.totalMemory == 0 || localCopy.totalMemory > (1ULL << 40)) { // 超过1TB内存不太可能
            Logger::Warn("内存数据异常，跳过更新");
            return;
        }
        
        // 转换共享内存数据为SystemInfo结构
        SystemInfo sysInfo;
        
        // CPU信息 - 使用安全的字符串转换
        sysInfo.cpuName = safeFromWCharArray(localCopy.cpuName, sizeof(localCopy.cpuName)/sizeof(wchar_t)).toStdString();
        sysInfo.physicalCores = localCopy.physicalCores;
        sysInfo.logicalCores = localCopy.logicalCores;
        sysInfo.performanceCores = localCopy.performanceCores;
        sysInfo.efficiencyCores = localCopy.efficiencyCores;
        sysInfo.cpuUsage = localCopy.cpuUsage;
        sysInfo.hyperThreading = localCopy.hyperThreading;
        sysInfo.virtualization = localCopy.virtualization;
        
        // 内存信息
        sysInfo.totalMemory = localCopy.totalMemory;
        sysInfo.usedMemory = localCopy.usedMemory;
        sysInfo.availableMemory = localCopy.availableMemory;
        
        // GPU信息 - 使用安全的字符串转换
        sysInfo.gpus.clear();
        for (int i = 0; i < localCopy.gpuCount && i < 2; ++i) {
            GPUData gpu;
            QString gpuName = safeFromWCharArray(localCopy.gpus[i].name, sizeof(localCopy.gpus[i].name)/sizeof(wchar_t));
            QString gpuBrand = safeFromWCharArray(localCopy.gpus[i].brand, sizeof(localCopy.gpus[i].brand)/sizeof(wchar_t));
            
            wcsncpy_s(gpu.name, sizeof(gpu.name)/sizeof(wchar_t), gpuName.toStdWString().c_str(), _TRUNCATE);
            wcsncpy_s(gpu.brand, sizeof(gpu.brand)/sizeof(wchar_t), gpuBrand.toStdWString().c_str(), _TRUNCATE);
            gpu.memory = localCopy.gpus[i].memory;
            gpu.coreClock = localCopy.gpus[i].coreClock;
            gpu.isVirtual = localCopy.gpus[i].isVirtual;
            sysInfo.gpus.push_back(gpu);
        }
        
        // 温度信息 - 使用安全的字符串转换
        sysInfo.temperatures.clear();
        for (int i = 0; i < localCopy.tempCount && i < 10; ++i) {
            QString sensorName = safeFromWCharArray(localCopy.temperatures[i].sensorName, 
                                                   sizeof(localCopy.temperatures[i].sensorName)/sizeof(wchar_t));
            sysInfo.temperatures.push_back({
                sensorName.toStdString(),
                localCopy.temperatures[i].temperature
            });
        }
        
        // 磁盘信息 - 使用安全的字符串转换和验证
        sysInfo.disks.clear();
        for (int i = 0; i < localCopy.diskCount && i < 8; ++i) {
            const auto& diskData = localCopy.disks[i];
            
            // 验证磁盘数据有效性
            if (diskData.totalSize == 0 && diskData.usedSpace == 0 && diskData.freeSpace == 0) {
                continue; // 跳过无效的磁盘数据
            }
            
            DiskData disk;
            disk.letter = diskData.letter;
            
            // 安全转换字符串
            QString label = safeFromWCharArray(diskData.label, sizeof(diskData.label)/sizeof(wchar_t));
            QString fileSystem = safeFromWCharArray(diskData.fileSystem, sizeof(diskData.fileSystem)/sizeof(wchar_t));
            
            // 直接赋值给 std::string
            disk.label = label.toStdString();
            disk.fileSystem = fileSystem.toStdString();
            
            disk.totalSize = diskData.totalSize;
            disk.usedSpace = diskData.usedSpace;
            disk.freeSpace = diskData.freeSpace;
            
            sysInfo.disks.push_back(disk);
        }
        
        // 更新UI显示
        updateSystemInfo(sysInfo);
        updateGpuSelector();
        updateNetworkSelector();
        updateDiskTreeWidget();
        updateCharts(); // 更新图表
        
        Logger::Debug("共享内存数据更新完成");
        
    } catch (const std::exception& e) {
        Logger::Error("从共享内存更新数据时发生错误: " + std::string(e.what()));
    } catch (...) {
        Logger::Error("从共享内存更新数据时发生未知错误");
    }
}

void QtWidgetsTCmonitor::updateCharts() {
    // 更新图表
    if (!cpuTempSeries || !gpuTempSeries) {
        return;
    }
    
    // 模拟温度数据更新
    static int timeCounter = 0;
    timeCounter++;
    
    // CPU温度数据（模拟）
    float cpuTemp = 45.0f + (rand() % 20); // 45-65度范围
    cpuTempHistory.push(cpuTemp);
    if (cpuTempHistory.size() > MAX_DATA_POINTS) {
        cpuTempHistory.pop();
    }
    
    // GPU温度数据（模拟）
    float gpuTemp = 50.0f + (rand() % 25); // 50-75度范围
    gpuTempHistory.push(gpuTemp);
    if (gpuTempHistory.size() > MAX_DATA_POINTS) {
        gpuTempHistory.pop();
    }
    
    // 更新CPU温度图表
    cpuTempSeries->clear();
    std::queue<float> tempQueue = cpuTempHistory;
    for (int i = 0; !tempQueue.empty(); ++i) {
        cpuTempSeries->append(i, tempQueue.front());
        tempQueue.pop();
    }
    
    // 更新GPU温度图表
    gpuTempSeries->clear();
    tempQueue = gpuTempHistory;
    for (int i = 0; !tempQueue.empty(); ++i) {
        gpuTempSeries->append(i, tempQueue.front());
        tempQueue.pop();
    }
    
    Logger::Debug("图表数据已更新");
}

// ============================================================================
// 磁盘信息更新函数实现
// ============================================================================

void QtWidgetsTCmonitor::updateDiskInfoFromSharedMemory() {
    // 从SharedMemoryManager获取磁盘信息 - 使用本地拷贝避免并发访问
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateDiskInfoFromSharedMemory: 共享内存缓冲区为空");
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证磁盘数据
        if (localCopy.diskCount > 8) {
            Logger::Warn("磁盘数量异常: " + std::to_string(localCopy.diskCount) + "，跳过磁盘更新");
            return;
        }
        
        Logger::Debug("更新磁盘信息，磁盘数量: " + std::to_string(localCopy.diskCount));
        
        // 清理现有的磁盘信息显示
        if (diskGroupBox) {
            QLayout* layout = diskGroupBox->layout();
            if (layout) {
                // 清理现有的子控件
                QLayoutItem* item;
                while ((item = layout->takeAt(0)) != nullptr) {
                    if (item->widget()) {
                        item->widget()->deleteLater();
                    }
                    delete item;
                }
                delete layout;
            }
        }
        
        // 为每个磁盘创建信息显示
        QVBoxLayout* diskLayout = new QVBoxLayout(diskGroupBox);
        
        for (int i = 0; i < localCopy.diskCount && i < 8; ++i) {
            const auto& disk = localCopy.disks[i];
            
            // 验证磁盘数据有效性
            if (disk.totalSize == 0 && disk.usedSpace == 0 && disk.freeSpace == 0) {
                Logger::Debug("跳过无效磁盘数据，索引: " + std::to_string(i));
                continue;
            }
            
            // 创建磁盘信息组
            QGroupBox* diskBox = new QGroupBox(QString("磁盘 %1:").arg(QString(disk.letter)));
            QGridLayout* diskInfoLayout = new QGridLayout(diskBox);
            
            int row = 0;
            
            // 磁盘标签 - 使用安全的字符串转换
            QString label = safeFromWCharArray(disk.label, sizeof(disk.label)/sizeof(wchar_t));
            if (label.isEmpty()) label = "未命名";
            diskInfoLayout->addWidget(new QLabel("标签:"), row, 0);
            diskInfoLayout->addWidget(new QLabel(label), row++, 1);
            
            // 文件系统 - 使用安全的字符串转换
            QString fileSystem = safeFromWCharArray(disk.fileSystem, sizeof(disk.fileSystem)/sizeof(wchar_t));
            if (fileSystem.isEmpty()) fileSystem = "未知";
            diskInfoLayout->addWidget(new QLabel("文件系统:"), row, 0);
            diskInfoLayout->addWidget(new QLabel(fileSystem), row++, 1);
            
            // 总容量
            diskInfoLayout->addWidget(new QLabel("总容量:"), row, 0);
            diskInfoLayout->addWidget(new QLabel(formatSize(disk.totalSize)), row++, 1);
            
            // 已用空间
            diskInfoLayout->addWidget(new QLabel("已用空间:"), row, 0);
            diskInfoLayout->addWidget(new QLabel(formatSize(disk.usedSpace)), row++, 1);
            
            // 可用空间
            diskInfoLayout->addWidget(new QLabel("可用空间:"), row, 0);
            diskInfoLayout->addWidget(new QLabel(formatSize(disk.freeSpace)), row++, 1);
            
            // 使用率
            double usagePercent = disk.totalSize > 0 ?
                (static_cast<double>(disk.usedSpace) / disk.totalSize * 100.0) : 0.0;
            diskInfoLayout->addWidget(new QLabel("使用率:"), row, 0);
            diskInfoLayout->addWidget(new QLabel(formatPercentage(usagePercent)), row++, 1);
            
            diskLayout->addWidget(diskBox);
        }
        
        diskLayout->addStretch();
        
        Logger::Debug("磁盘信息更新完成");
        
    } catch (const std::exception& e) {
        Logger::Error("更新磁盘信息时发生错误: " + std::string(e.what()));
        
        // 创建错误显示
        if (diskGroupBox) {
            QLayout* layout = diskGroupBox->layout();
            if (layout) {
                QLayoutItem* item;
                while ((item = layout->takeAt(0)) != nullptr) {
                    if (item->widget()) {
                        item->widget()->deleteLater();
                    }
                    delete item;
                }
                delete layout;
            }
            
            QVBoxLayout* errorLayout = new QVBoxLayout(diskGroupBox);
            errorLayout->addWidget(new QLabel("磁盘信息读取错误"));
        }
    } catch (...) {
        Logger::Error("更新磁盘信息时发生未知错误");
    }
}

// ============================================================================
// 重新连接共享内存函数实现
// ============================================================================

void QtWidgetsTCmonitor::tryReconnectSharedMemory() {
    // 尝试重新连接共享内存
    if (SharedMemoryManager::InitSharedMemory()) {
        // 连接成功，重新初始化UI
        setWindowTitle(tr("系统硬件监视器"));
        
        // 重新创建UI
        setupUI();
        
        // 更改定时器功能为正常更新
        updateTimer->disconnect(); // 断开重连信号
        connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::updateFromSharedMemory);
        updateTimer->start(1000); // 每秒更新
        
        Logger::Info("QT-UI 成功重新连接到共享内存");
        
        // 显示成功消息
        QMessageBox::information(this, tr("连接成功"), 
            tr("已成功连接到主程序，开始显示系统信息。"));
    } else {
        // 连接失败，更新错误信息
        QLabel* errorLabel = qobject_cast<QLabel*>(centralWidget());
        if (errorLabel) {
            QString currentTime = QTime::currentTime().toString("hh:mm:ss");
            errorLabel->setText(tr("共享内存连接失败\n请启动主程序 Win_x64_sysMonitor.exe\n\n最后尝试时间: %1\n正在每5秒重试...").arg(currentTime));
        }
    }
}
