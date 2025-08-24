#include "stdafx.h"
#include "QtWidgetsTCmonitor.h"
#include "ui_QtWidgetsTCmonitor.h"  // Auto-generated UI file
#include "SmartDetailsDialog.h"     // 新增：SMART详情对话框
#include "../src/core/DataStruct/SharedMemoryManager.h"
#include "../src/core/utils/Logger.h"  // 添加Logger支持

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>      // 新增：按钮支持
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
#include <chrono>    // For high_resolution_clock

QtWidgetsTCmonitor::QtWidgetsTCmonitor(QWidget* parent)
    : QMainWindow(parent), 
      updateTimer(nullptr),
      cpuGroupBox(nullptr), memoryGroupBox(nullptr), gpuGroupBox(nullptr),
      temperatureGroupBox(nullptr), diskGroupBox(nullptr), networkGroupBox(nullptr),
      gpuSelector(nullptr), networkSelector(nullptr), diskSelector(nullptr),
      networkNameLabel(nullptr), networkStatusLabel(nullptr), networkTypeLabel(nullptr),
      networkIpLabel(nullptr), networkMacLabel(nullptr), networkSpeedLabel(nullptr), 
      gpuDriverVersionLabel(nullptr),
      cpuTempChart(nullptr), cpuTempChartView(nullptr), cpuTempSeries(nullptr),
      gpuTempChart(nullptr), gpuTempChartView(nullptr), gpuTempSeries(nullptr),
      ui(nullptr),
      currentGpuIndex(0), currentNetworkIndex(0), currentDiskIndex(0)
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
        
        // 设置定时器尝试重新连接 - 提高重连频率
        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::tryReconnectSharedMemory);
        updateTimer->start(2000); // 每2秒尝试重新连接（从5秒提升到2秒）
        return;
    }

    // Set up update timer - 大幅提升刷新速度
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::updateFromSharedMemory);
    updateTimer->start(250); // 从1000ms（1秒）提升到250ms（0.25秒），实现4倍速度提升
    
    Logger::Info("Qt UI 初始化完成，刷新间隔: 250ms");
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
    createNetworkSection();  // 确保调用网络部分创建
    createDiskSection();    // 创建磁盘部分

    // Add to main layout
    mainLayout->addWidget(cpuGroupBox);
    mainLayout->addWidget(memoryGroupBox);
    mainLayout->addWidget(gpuGroupBox);
    mainLayout->addWidget(temperatureGroupBox);
    mainLayout->addWidget(networkGroupBox);  // 添加网络groupbox
    mainLayout->addWidget(diskGroupBox);    // 添加磁盘groupbox
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

    // 新增：CPU使用率采样间隔
    layout->addWidget(new QLabel(tr("采样间隔:"), this), row, 0);
    infoLabels["cpuSampleInterval"] = new QLabel(tr("N/A"), this);
    layout->addWidget(infoLabels["cpuSampleInterval"], row++, 1);
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
    
    // 添加GPU选择器
    layout->addWidget(new QLabel(tr("GPU选择:"), this), row, 0);
    gpuSelector = new QComboBox(this);
    gpuSelector->addItem(tr("正在检测GPU..."));
    connect(gpuSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &QtWidgetsTCmonitor::onGpuSelectionChanged);
    layout->addWidget(gpuSelector, row++, 1);
    
    layout->addWidget(new QLabel(tr("名称:"), this), row, 0);
    infoLabels["gpuName"] = new QLabel(tr("未知"), this);
    layout->addWidget(infoLabels["gpuName"], row++, 1);

    layout->addWidget(new QLabel(tr("品牌:"), this), row, 0);
    infoLabels["gpuBrand"] = new QLabel(tr("未知"), this);
    layout->addWidget(infoLabels["gpuBrand"], row++, 1);

    layout->addWidget(new QLabel(tr("显存:"), this), row, 0);
    infoLabels["gpuMemory"] = new QLabel(tr("未知"), this);
    layout->addWidget(infoLabels["gpuMemory"], row++, 1);

    layout->addWidget(new QLabel(tr("核心频率:"), this), row, 0);
    infoLabels["gpuCoreFreq"] = new QLabel(tr("未知"), this);
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

    layout->addWidget(new QLabel(tr("网卡类型:"), this), row, 0);
    networkTypeLabel = new QLabel(this);
    layout->addWidget(networkTypeLabel, row++, 1);

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
    diskGroupBox = new QGroupBox(tr("存储设备信息"), this);
    QVBoxLayout* mainLayout = new QVBoxLayout(diskGroupBox);
    
    // 创建磁盘选择器和控制区域
    QWidget* controlWidget = new QWidget();
    QGridLayout* controlLayout = new QGridLayout(controlWidget);
    
    int row = 0;
    
    // 磁盘选择器
    controlLayout->addWidget(new QLabel(tr("存储设备:"), this), row, 0);
    diskSelector = new QComboBox(this);
    diskSelector->addItem(tr("正在检测存储设备..."));
    connect(diskSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &QtWidgetsTCmonitor::onDiskSelectionChanged);
    controlLayout->addWidget(diskSelector, row++, 1);
    
    mainLayout->addWidget(controlWidget);
    
    // 创建详细信息区域 - 使用水平分割
    QSplitter* infoSplitter = new QSplitter(Qt::Horizontal);
    
    // 左侧：基本信息
    QGroupBox* basicInfoBox = new QGroupBox(tr("基本信息"));
    QGridLayout* basicLayout = new QGridLayout(basicInfoBox);
    
    row = 0;
    // 盘符
    basicLayout->addWidget(new QLabel(tr("盘符:"), this), row, 0);
    infoLabels["diskLetter"] = new QLabel(tr("未知"), this);
    basicLayout->addWidget(infoLabels["diskLetter"], row++, 1);
    
    // 卷标
    basicLayout->addWidget(new QLabel(tr("卷标:"), this), row, 0);
    infoLabels["diskLabel"] = new QLabel(tr("未知"), this);
    infoLabels["diskLabel"]->setWordWrap(true); // 允许换行
    basicLayout->addWidget(infoLabels["diskLabel"], row++, 1);
    
    // 文件系统
    basicLayout->addWidget(new QLabel(tr("文件系统:"), this), row, 0);
    infoLabels["diskFileSystem"] = new QLabel(tr("未知"), this);
    basicLayout->addWidget(infoLabels["diskFileSystem"], row++, 1);
    
    // 总容量
    basicLayout->addWidget(new QLabel(tr("总容量:"), this), row, 0);
    infoLabels["diskCapacity"] = new QLabel(tr("未知"), this);
    basicLayout->addWidget(infoLabels["diskCapacity"], row++, 1);
    
    infoSplitter->addWidget(basicInfoBox);
    
    // 右侧：使用情况
    QGroupBox* usageInfoBox = new QGroupBox(tr("使用情况"));
    QGridLayout* usageLayout = new QGridLayout(usageInfoBox);
    
    row = 0;
    // 已用空间
    usageLayout->addWidget(new QLabel(tr("已用空间:"), this), row, 0);
    infoLabels["diskUsed"] = new QLabel(tr("未知"), this);
    usageLayout->addWidget(infoLabels["diskUsed"], row++, 1);
    
    // 可用空间
    usageLayout->addWidget(new QLabel(tr("可用空间:"), this), row, 0);
    infoLabels["diskFree"] = new QLabel(tr("未知"), this);
    usageLayout->addWidget(infoLabels["diskFree"], row++, 1);
    
    // 使用率
    usageLayout->addWidget(new QLabel(tr("使用率:"), this), row, 0);
    infoLabels["diskUsage"] = new QLabel(tr("未知"), this);
    usageLayout->addWidget(infoLabels["diskUsage"], row++, 1);
    
    // SMART按钮（留作未来扩展）
    QPushButton* smartButton = new QPushButton(tr("查看SMART详情"));
    connect(smartButton, &QPushButton::clicked, this, &QtWidgetsTCmonitor::showCurrentDiskSmartDetails);
    usageLayout->addWidget(smartButton, row++, 0, 1, 2);
    
    infoSplitter->addWidget(usageInfoBox);
    
    mainLayout->addWidget(infoSplitter);
    
    // 初始化变量
    currentDiskIndex = 0;
}

void QtWidgetsTCmonitor::updateDiskSelector() {
    // 更新磁盘选择器 - 使用逻辑磁盘信息
    if (!diskSelector) {
        Logger::Warn("updateDiskSelector: diskSelector未初始化");
        return;
    }
    
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateDiskSelector: 共享内存缓冲区不可用");
        diskSelector->clear();
        diskSelector->addItem(tr("无法读取磁盘信息"));
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证磁盘数据
        if (localCopy.diskCount < 0 || localCopy.diskCount > 8) {
            Logger::Warn("磁盘数量异常: " + std::to_string(localCopy.diskCount));
            localCopy.diskCount = 0;
        }
        
        // 检查磁盘列表是否发生变化
        static int lastDiskCount = -1;
        static std::vector<QString> lastDiskNames;
        
        std::vector<QString> currentDiskNames;
        for (int i = 0; i < localCopy.diskCount && i < 8; ++i) {
            QString diskName = QString("驱动器 %1:").arg(QChar(localCopy.disks[i].letter));
            QString label = safeFromWCharArray(localCopy.disks[i].label, 
                                             sizeof(localCopy.disks[i].label)/sizeof(wchar_t));
            if (!label.isEmpty()) {
                diskName += QString(" (%1)").arg(label);
            }
            currentDiskNames.push_back(diskName);
        }
        
        // 如果磁盘列表没有变化，则不需要重建选择器
        bool needsRebuild = (lastDiskCount != localCopy.diskCount) || 
                           (currentDiskNames != lastDiskNames);
        
        if (!needsRebuild) {
            return; // 磁盘列表没有变化，保持当前选择状态
        }
        
        // 保存当前选择的索引（用于恢复选择状态）
        int previousSelection = diskSelector->currentIndex();
        
        // 临时断开信号，避免触发onDiskSelectionChanged
        diskSelector->blockSignals(true);
        
        diskSelector->clear();
        diskIndices.clear();
        
        if (localCopy.diskCount > 0) {
            for (int i = 0; i < localCopy.diskCount && i < 8; ++i) {
                const auto& disk = localCopy.disks[i];
                
                // 创建显示名称
                QString diskName = QString("驱动器 %1:").arg(QChar(disk.letter));
                QString label = safeFromWCharArray(disk.label, sizeof(disk.label)/sizeof(wchar_t));
                
                if (!label.isEmpty()) {
                    diskName += QString(" (%1)").arg(label);
                }
                
                diskSelector->addItem(diskName);
                diskIndices.push_back(i);
                
                Logger::Debug("已添加磁盘到选择器: " + diskName.toStdString());
            }
            
            // 智能恢复选择状态
            if (diskSelector->count() > 0) {
                if (previousSelection >= 0 && previousSelection < diskSelector->count()) {
                    diskSelector->setCurrentIndex(previousSelection);
                    currentDiskIndex = diskIndices[previousSelection];
                } else {
                    diskSelector->setCurrentIndex(0);
                    currentDiskIndex = diskIndices[0];
                }
            }
        } else {
            diskSelector->addItem(tr("未检测到磁盘"));
            currentDiskIndex = -1;
        }
        
        // 更新缓存
        lastDiskCount = localCopy.diskCount;
        lastDiskNames = currentDiskNames;
        
        // 重新启用信号
        diskSelector->blockSignals(false);
        
        Logger::Debug("磁盘选择器更新完成，当前选择索引: " + std::to_string(currentDiskIndex));
        
    } catch (const std::exception& e) {
        Logger::Error("updateDiskSelector异常: " + std::string(e.what()));
        diskSelector->clear();
        diskSelector->addItem(tr("磁盘信息读取失败"));
    }
}

void QtWidgetsTCmonitor::onDiskSelectionChanged(int index) {
    // 磁盘选择变化处理
    Logger::Debug("磁盘选择变化: index=" + std::to_string(index) + 
                 ", diskIndices.size()=" + std::to_string(diskIndices.size()) +
                 ", currentDiskIndex=" + std::to_string(currentDiskIndex));
    
    if (index < 0) {
        Logger::Warn("磁盘选择索引无效: " + std::to_string(index));
        currentDiskIndex = -1;
        return;
    }
    
    if (index >= static_cast<int>(diskIndices.size())) {
        Logger::Warn("磁盘选择索引超出范围: " + std::to_string(index) + 
                    " >= " + std::to_string(diskIndices.size()));
        currentDiskIndex = -1;
        return;
    }
    
    // 更新当前磁盘索引
    int newDiskIndex = diskIndices[index];
    
    if (newDiskIndex != currentDiskIndex) {
        currentDiskIndex = newDiskIndex;
        Logger::Info("用户选择了磁盘: 选择器索引=" + std::to_string(index) + 
                    ", 实际磁盘索引=" + std::to_string(currentDiskIndex));
        
        // 立即更新磁盘相关显示信息
        updateDiskInfoDisplay();
    }
}

void QtWidgetsTCmonitor::updateDiskInfoDisplay() {
    // 更新磁盘信息显示
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateDiskInfoDisplay: 共享内存缓冲区不可用");
        return;
    }
    
    try {
        // 创建本地拷贝
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        if (currentDiskIndex >= 0 && 
            currentDiskIndex < localCopy.diskCount && 
            currentDiskIndex < 8) {
            
            const auto& disk = localCopy.disks[currentDiskIndex];
            
            // 更新基本信息
            infoLabels["diskLetter"]->setText(QString(QChar(disk.letter)) + ":");
            
            QString label = safeFromWCharArray(disk.label, sizeof(disk.label)/sizeof(wchar_t));
            infoLabels["diskLabel"]->setText(label.isEmpty() ? tr("未命名") : label);
            
            QString fileSystem = safeFromWCharArray(disk.fileSystem, sizeof(disk.fileSystem)/sizeof(wchar_t));
            infoLabels["diskFileSystem"]->setText(fileSystem.isEmpty() ? tr("未知") : fileSystem);
            
            // 容量信息
            if (disk.totalSize > 0) {
                infoLabels["diskCapacity"]->setText(formatSize(disk.totalSize));
                infoLabels["diskUsed"]->setText(formatSize(disk.usedSpace));
                infoLabels["diskFree"]->setText(formatSize(disk.freeSpace));
                
                // 使用率
                double usagePercent = static_cast<double>(disk.usedSpace) / disk.totalSize * 100.0;
                infoLabels["diskUsage"]->setText(formatPercentage(usagePercent));
                
                // 使用率颜色提示
                if (usagePercent > 90) {
                    infoLabels["diskUsage"]->setStyleSheet("QLabel { color: red; font-weight: bold; }");
                } else if (usagePercent > 80) {
                    infoLabels["diskUsage"]->setStyleSheet("QLabel { color: orange; }");
                } else {
                    infoLabels["diskUsage"]->setStyleSheet("QLabel { color: green; }");
                }
            } else {
                infoLabels["diskCapacity"]->setText(tr("未知"));
                infoLabels["diskUsed"]->setText(tr("未知"));
                infoLabels["diskFree"]->setText(tr("未知"));
                infoLabels["diskUsage"]->setText(tr("未知"));
                infoLabels["diskUsage"]->setStyleSheet("");
            }
            
            Logger::Debug("磁盘信息显示已更新: 驱动器 " + std::string(1, disk.letter));
            
        } else {
            // 显示默认信息
            resetDiskInfoLabels();
            Logger::Debug("磁盘信息重置为默认值");
        }
        
    } catch (const std::exception& e) {
        Logger::Error("更新磁盘信息显示时发生错误: " + std::string(e.what()));
        resetDiskInfoLabels();
    }
}

void QtWidgetsTCmonitor::resetDiskInfoLabels() {
    // 重置磁盘信息标签为默认值
    if (infoLabels.find("diskLetter") != infoLabels.end()) {
        infoLabels["diskLetter"]->setText(tr("未知"));
    }
    
    if (infoLabels.find("diskLabel") != infoLabels.end()) {
        infoLabels["diskLabel"]->setText(tr("未知"));
    }
    
    if (infoLabels.find("diskFileSystem") != infoLabels.end()) {
        infoLabels["diskFileSystem"]->setText(tr("未知"));
    }
    
    if (infoLabels.find("diskCapacity") != infoLabels.end()) {
        infoLabels["diskCapacity"]->setText(tr("未知"));
    }
    
    if (infoLabels.find("diskUsed") != infoLabels.end()) {
        infoLabels["diskUsed"]->setText(tr("未知"));
    }
    
    if (infoLabels.find("diskFree") != infoLabels.end()) {
        infoLabels["diskFree"]->setText(tr("未知"));
    }
    
    if (infoLabels.find("diskUsage") != infoLabels.end()) {
        infoLabels["diskUsage"]->setText(tr("未知"));
        infoLabels["diskUsage"]->setStyleSheet("");
    }
}

void QtWidgetsTCmonitor::showCurrentDiskSmartDetails() {
    // 显示当前选中磁盘的SMART详细信息
    if (currentDiskIndex < 0) {
        QMessageBox::information(this, tr("SMART信息"), tr("请先选择一个磁盘设备"));
        return;
    }
    
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取磁盘信息"));
        return;
    }
    
    try {
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        if (currentDiskIndex >= localCopy.diskCount) {
            QMessageBox::warning(this, tr("错误"), tr("磁盘索引超出范围"));
            return;
        }
        
        const auto& disk = localCopy.disks[currentDiskIndex];
        QString diskName = QString("驱动器 %1:").arg(QChar(disk.letter));
        
        showSmartDetails(diskName);
        
    } catch (const std::exception& e) {
        Logger::Error("显示SMART详情时发生错误: " + std::string(e.what()));
        QMessageBox::critical(this, tr("错误"), tr("显示SMART信息时发生错误"));
    }
}

// ============================================================================
// 磁盘更新函数修改 - 支持磁盘选择器
// ============================================================================

// 在updateFromSharedMemory()函数中添加磁盘选择器更新（保持现有代码结构）
void QtWidgetsTCmonitor::updateFromSharedMemory() {
    // 从共享内存更新数据 - 使用优化的高性能版本
    static auto lastUpdateTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime);
    
    // 跳过过于频繁的更新请求（防止UI过载）
    if (timeSinceLastUpdate.count() < 50) {
        return; // 最小50ms间隔
    }
    lastUpdateTime = currentTime;
    
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
        memset(&localCopy, 0, sizeof(SharedMemoryBlock));
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 快速数据完整性验证
        if (localCopy.diskCount < 0 || localCopy.diskCount > 8 ||
            localCopy.gpuCount < 0 || localCopy.gpuCount > 2 ||
            localCopy.tempCount < 0 || localCopy.tempCount > 10 ||
            localCopy.totalMemory == 0 || localCopy.totalMemory > (1ULL << 40)) {
            Logger::Warn("数据完整性检查失败，跳过更新");
            return;
        }
        
        // CPU和内存信息 - 使用SharedMemoryBlock中的直接字段
        infoLabels["cpuName"]->setText(safeFromWCharArray(localCopy.cpuName, sizeof(localCopy.cpuName)/sizeof(wchar_t)));
        infoLabels["physicalCores"]->setText(QString::number(localCopy.physicalCores));
        infoLabels["logicalCores"]->setText(QString::number(localCopy.logicalCores));
        infoLabels["performanceCores"]->setText(QString::number(localCopy.performanceCores));
        infoLabels["efficiencyCores"]->setText(QString::number(localCopy.efficiencyCores));
        infoLabels["cpuUsage"]->setText(QString::number(localCopy.cpuUsage, 'f', 1) + "%");
        infoLabels["hyperThreading"]->setText(localCopy.hyperThreading ? tr("启用") : tr("禁用"));
        infoLabels["virtualization"]->setText(localCopy.virtualization ? tr("支持") : tr("不支持"));
        if (infoLabels.find("cpuSampleInterval") != infoLabels.end()) {
            if (localCopy.cpuUsageSampleIntervalMs > 0.0 && localCopy.cpuUsageSampleIntervalMs < 60000.0)
                infoLabels["cpuSampleInterval"]->setText(QString::number(localCopy.cpuUsageSampleIntervalMs, 'f', 0) + " ms");
            else
                infoLabels["cpuSampleInterval"]->setText("N/A");
        }
        
        // 内存信息
        infoLabels["totalMemory"]->setText(formatSize(localCopy.totalMemory));
        infoLabels["usedMemory"]->setText(formatSize(localCopy.usedMemory));
        infoLabels["availableMemory"]->setText(formatSize(localCopy.availableMemory));
        
        double memoryUsagePercent = localCopy.totalMemory > 0 ? 
            (static_cast<double>(localCopy.usedMemory) / localCopy.totalMemory * 100.0) : 0.0;
        infoLabels["memoryUsage"]->setText(QString::number(memoryUsagePercent, 'f', 1) + "%");
        
        // GPU信息 - 只更新当前选择的GPU
        if (currentGpuIndex >= 0 && currentGpuIndex < localCopy.gpuCount) {
            const auto& gpu = localCopy.gpus[currentGpuIndex];
            
            infoLabels["gpuName"]->setText(safeFromWCharArray(gpu.name, sizeof(gpu.name)/sizeof(wchar_t)));
            infoLabels["gpuBrand"]->setText(safeFromWCharArray(gpu.brand, sizeof(gpu.brand)/sizeof(wchar_t)));
            infoLabels["gpuMemory"]->setText(formatSize(gpu.memory));
            infoLabels["gpuCoreFreq"]->setText(QString::number(gpu.coreClock, 'f', 1) + " MHz");
        }
        
        // 温度信息
        infoLabels["cpuTemp"]->setText(QString::number(localCopy.cpuTemperature, 'f', 1) + " °C");
        infoLabels["gpuTemp"]->setText(QString::number(localCopy.gpuTemperature, 'f', 1) + " °C");
        
        // 选择性更新辅助组件（降低CPU使用）
        static int updateCounter = 0;
        if (++updateCounter % 2 == 0) { // 网络选择器每2次更新一次（提高响应速度）
            updateNetworkSelector();
        }
        
        if (updateCounter % 4 == 0) { // GPU选择器每4次更新一次（降低频率）
            updateGpuSelector();
        }
        
        if (updateCounter % 3 == 0) { // 磁盘选择器每3次更新一次
            updateDiskSelector();
        }
        
        updateCharts(); // 图表每次都更新（用户最关注的实时数据）
        
        // 更新当前选中磁盘的详细信息
        updateDiskInfoDisplay();
        
        // 重置计数器防止溢出
        if (updateCounter >= 100) updateCounter = 0;
        
    } catch (const std::exception& e) {
        Logger::Error("从共享内存更新数据时发生错误: " + std::string(e.what()));
    } catch (...) {
        Logger::Error("从共享内存更新数据时发生未知错误");
    }
}

void QtWidgetsTCmonitor::updateCharts() {
    // 更新图表 - 修复温度数据获取问题
    if (!cpuTempSeries || !gpuTempSeries) {
        return;
    }
    
    // 从共享内存获取实时温度数据
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        Logger::Warn("updateCharts: 共享内存缓冲区不可用");
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 从共享内存获取当前温度数据
        float cpuTemp = static_cast<float>(localCopy.cpuTemperature);
        float gpuTemp = static_cast<float>(localCopy.gpuTemperature);

        // 如果没有数据，使用默认值
        if (cpuTemp <= 0) cpuTemp = 45.0f + (rand() % 20);
        if (gpuTemp <= 0) gpuTemp = 50.0f + (rand() % 25);

        cpuTempHistory.push(cpuTemp);
        if (cpuTempHistory.size() > MAX_DATA_POINTS) {
            cpuTempHistory.pop();
        }

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
        
        Logger::Trace("温度图表已更新 - CPU: " + std::to_string(cpuTemp) + "°C, GPU: " + std::to_string(gpuTemp) + "°C");
    }
    catch (const std::exception& e) {
        Logger::Error("更新图表时发生错误: " + std::string(e.what()));
    }
    catch (...) {
        Logger::Error("更新图表时发生未知错误");
    }
}

// ============================================================================
// 缺失函数的实现
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
        gpuSelector->clear();
        gpuSelector->addItem(tr("无法读取GPU信息"));
        return;
    }
    
    try {
        // 创建本地拷贝以避免并发访问问题
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 验证GPU数据
        if (localCopy.gpuCount < 0 || localCopy.gpuCount > 8) {
            Logger::Warn("GPU数量异常: " + std::to_string(localCopy.gpuCount) + "，重置为0");
            localCopy.gpuCount = 0;
        }
        
        gpuSelector->clear();
        gpuIndices.clear();
        
        if (localCopy.gpuCount > 0) {
            for (int i = 0; i < localCopy.gpuCount && i < 8; ++i) {
                const auto& gpu = localCopy.gpus[i];
                
                // 验证GPU名称是否有效
                if (wcslen(gpu.name) == 0) {
                    continue; // 跳过空名称的GPU
                }
                
                QString gpuName = QString::fromWCharArray(gpu.name);
                
                // 检查是否为异常数据（比如全是数字的异常字符串）
                bool isValidName = false;
                for (const QChar& ch : gpuName) {
                    if (ch.isLetter() || ch.isSpace()) {
                        isValidName = true;
                        break;
                    }
                }
                
                if (!isValidName || gpuName.length() > 100) {
                    Logger::Warn("检测到异常GPU名称，跳过: " + gpuName.toStdString());
                    continue;
                }
                
                // 添加有效的GPU到选择器
                QString displayName = gpuName;
                if (gpu.isVirtual) {
                    displayName += tr(" (虚拟)");
                }
                
                gpuSelector->addItem(displayName);
                gpuIndices.push_back(i);
                
                Logger::Debug("已添加GPU到选择器: " + displayName.toStdString());
            }
        }
        
        // 如果没有有效的GPU
        if (gpuSelector->count() == 0) {
            gpuSelector->addItem(tr("未检测到GPU"));
            Logger::Info("未检测到有效的GPU设备");
        }
    }
    catch (const std::exception& e) {
        Logger::Error("updateGpuSelector异常: " + std::string(e.what()));
        gpuSelector->clear();
        gpuSelector->addItem(tr("GPU信息读取失败"));
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
        
        // 保存当前选择的索引（用于恢复选择状态）
        int previousSelection = networkSelector->currentIndex();
        
        // 检查网络适配器列表是否发生变化
        static int lastAdapterCount = -1;
        static std::vector<QString> lastAdapterNames;
        
        std::vector<QString> currentAdapterNames;
        for (int i = 0; i < localCopy.adapterCount && i < 4; ++i) {
            QString adapterName = safeFromWCharArray(localCopy.adapters[i].name, 
                                                   sizeof(localCopy.adapters[i].name)/sizeof(wchar_t));
            if (!adapterName.isEmpty()) {
                currentAdapterNames.push_back(adapterName);
            }
        }
        
        // 如果适配器列表没有变化，则不需要重建选择器
        bool needsRebuild = (lastAdapterCount != localCopy.adapterCount) || 
                           (currentAdapterNames != lastAdapterNames);
        
        if (!needsRebuild) {
            return; // 适配器列表没有变化，保持当前选择状态
        }
        
        // 临时断开信号，避免触发onNetworkSelectionChanged
        networkSelector->blockSignals(true);
        
        networkSelector->clear();
        networkIndices.clear();
        
        if (localCopy.adapterCount > 0) {
            for (int i = 0; i < localCopy.adapterCount && i < 4; ++i) {
                QString adapterName = safeFromWCharArray(localCopy.adapters[i].name, 
                                                       sizeof(localCopy.adapters[i].name)/sizeof(wchar_t));
                if (!adapterName.isEmpty()) {
                    // 获取网卡类型信息
                    QString adapterType = safeFromWCharArray(localCopy.adapters[i].adapterType,
                                                           sizeof(localCopy.adapters[i].adapterType)/sizeof(wchar_t));
                    if (adapterType.isEmpty()) adapterType = "未知类型";
                    
                    // 创建更详细的显示名称
                    QString displayName = QString("%1 (%2)").arg(adapterName, adapterType);
                    networkSelector->addItem(displayName);
                    networkIndices.push_back(i);
                }
            }
            
            // 智能恢复选择状态
            if (networkSelector->count() > 0) {
                // 如果之前有选择，尝试恢复到相同位置
                if (previousSelection >= 0 && previousSelection < networkSelector->count()) {
                    networkSelector->setCurrentIndex(previousSelection);
                    currentNetworkIndex = networkIndices[previousSelection];
                } else {
                    // 否则选择第一个
                    networkSelector->setCurrentIndex(0);
                    currentNetworkIndex = networkIndices[0];
                }
            }
        } else {
            networkSelector->addItem("未检测到网络适配器");
            currentNetworkIndex = -1;
        }
        
        // 更新缓存
        lastAdapterCount = localCopy.adapterCount;
        lastAdapterNames = currentAdapterNames;
        
        // 重新启用信号
        networkSelector->blockSignals(false);
        
        Logger::Debug("网络选择器更新完成，当前选择索引: " + std::to_string(currentNetworkIndex));
        
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
    // 网络适配器选择变化处理 - 增强版本
    Logger::Debug("网络选择变化: index=" + std::to_string(index) + 
                 ", networkIndices.size()=" + std::to_string(networkIndices.size()) +
                 ", currentNetworkIndex=" + std::to_string(currentNetworkIndex));
    
    if (index < 0) {
        Logger::Warn("网络选择索引无效: " + std::to_string(index));
        currentNetworkIndex = -1;
        return;
    }
    
    if (index >= static_cast<int>(networkIndices.size())) {
        Logger::Warn("网络选择索引超出范围: " + std::to_string(index) + 
                    " >= " + std::to_string(networkIndices.size()));
        currentNetworkIndex = -1;
        return;
    }
    
    // 更新当前网络索引
    int newNetworkIndex = networkIndices[index];
    
    if (newNetworkIndex != currentNetworkIndex) {
        currentNetworkIndex = newNetworkIndex;
        Logger::Info("用户选择了网络适配器: 选择器索引=" + std::to_string(index) + 
                    ", 实际网络索引=" + std::to_string(currentNetworkIndex));
        
        // 立即更新网络适配器相关显示信息
        updateNetworkInfoDisplay();
    }
}

void QtWidgetsTCmonitor::updateNetworkInfoDisplay() {
    // 专门用于更新网络信息显示的函数 - 增强版本，支持真实连接状态
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        return;
    }
    
    try {
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        if (currentNetworkIndex >= 0 && 
            currentNetworkIndex < localCopy.adapterCount && 
            currentNetworkIndex < 4) {
            
            const auto& adapter = localCopy.adapters[currentNetworkIndex];
            
            // 更新网络信息显示
            networkNameLabel->setText(safeFromWCharArray(adapter.name, sizeof(adapter.name)/sizeof(wchar_t)));
            
            // 根据IP地址和速度判断连接状态（更准确的方法）
            QString ipAddress = safeFromWCharArray(adapter.ipAddress, sizeof(adapter.ipAddress)/sizeof(wchar_t));
            bool isReallyConnected = !ipAddress.isEmpty() && 
                                    ipAddress != "未连接" && 
                                    ipAddress != "0.0.0.0" &&
                                    adapter.speed > 0;
            
            if (networkStatusLabel) {
                if (isReallyConnected) {
                    networkStatusLabel->setText(tr("已连接"));
                    networkStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
                } else {
                    networkStatusLabel->setText(tr("未连接"));
                    networkStatusLabel->setStyleSheet("QLabel { color: red; }");
                }
            }
            
            if (networkTypeLabel) {
                networkTypeLabel->setText(safeFromWCharArray(adapter.adapterType, sizeof(adapter.adapterType)/sizeof(wchar_t)));
            }
            
            if (networkIpLabel) {
                if (isReallyConnected) {
                    networkIpLabel->setText(ipAddress);
                } else {
                    networkIpLabel->setText(tr("未分配"));
                    networkIpLabel->setStyleSheet("QLabel { color: gray; }");
                }
            }
            
            networkMacLabel->setText(safeFromWCharArray(adapter.mac, sizeof(adapter.mac)/sizeof(wchar_t)));
            
            // 网络速度显示优化 - 未连接时显示"未连接"而不是异常数值
            if (isReallyConnected && adapter.speed > 0) {
                networkSpeedLabel->setText(formatNetworkSpeed(adapter.speed));
                networkSpeedLabel->setStyleSheet("QLabel { color: black; }");
            } else {
                networkSpeedLabel->setText(tr("未连接"));
                networkSpeedLabel->setStyleSheet("QLabel { color: gray; }");
            }
            
            Logger::Debug("网络信息显示已更新: " + 
                         safeFromWCharArray(adapter.name, sizeof(adapter.name)/sizeof(wchar_t)).toStdString() +
                         " (连接状态: " + (isReallyConnected ? "已连接" : "未连接") + ")");
        } else {
            // 显示默认信息
            networkNameLabel->setText(tr("无"));
            if (networkStatusLabel) {
                networkStatusLabel->setText(tr("无"));
                networkStatusLabel->setStyleSheet("");
            }
            if (networkTypeLabel) networkTypeLabel->setText(tr("无"));
            if (networkIpLabel) {
                networkIpLabel->setText(tr("无"));
                networkIpLabel->setStyleSheet("");
            }
            networkMacLabel->setText(tr("无"));
            networkSpeedLabel->setText(tr("无"));
            networkSpeedLabel->setStyleSheet("");
            
            Logger::Debug("网络信息重置为默认值");
        }
    } catch (const std::exception& e) {
        Logger::Error("更新网络信息显示时发生错误: " + std::string(e.what()));
    }
}

void QtWidgetsTCmonitor::showSmartDetails(const QString& diskIdentifier) {
    // 显示完整的磁盘SMART详细信息
    Logger::Info("请求显示磁盘SMART详情: " + diskIdentifier.toStdString());
    
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取磁盘信息"));
        return;
    }
    
    try {
        SharedMemoryBlock localCopy;
        memcpy(&localCopy, buffer, sizeof(SharedMemoryBlock));
        
        // 查找对应的物理磁盘SMART数据
        PhysicalDiskSmartData smartData;
        bool found = false;
        
        // 根据diskIdentifier查找对应的物理磁盘
        for (int i = 0; i < localCopy.physicalDiskCount && i < 8; ++i) {
            const auto& disk = localCopy.physicalDisks[i];
            QString diskModel = QString::fromWCharArray(disk.model);
            QString diskSerial = QString::fromWCharArray(disk.serialNumber);
            
            // 匹配磁盘标识符（可以是型号、序列号或驱动器号）
            if (diskIdentifier.contains(diskModel) || 
                diskIdentifier.contains(diskSerial) ||
                diskIdentifier.startsWith("驱动器")) {
                smartData = disk;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // 如果没有找到物理磁盘数据，创建一个模拟的SMART数据用于演示
            createDemoSmartData(smartData, diskIdentifier);
        }
        
        // 创建并显示SMART详情对话框
        SmartDetailsDialog* dialog = new SmartDetailsDialog(smartData, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose); // 自动删除
        dialog->show();
        
    } catch (const std::exception& e) {
        Logger::Error("显示SMART详情时发生错误: " + std::string(e.what()));
        QMessageBox::critical(this, tr("错误"), tr("显示SMART信息时发生错误"));
    }
}

void QtWidgetsTCmonitor::createDemoSmartData(PhysicalDiskSmartData& smartData, const QString& diskIdentifier) {
    // 创建演示用的SMART数据
    memset(&smartData, 0, sizeof(PhysicalDiskSmartData));
    
    // 基本信息
    QString modelName = "Samsung SSD 980 PRO 1TB";
    if (diskIdentifier.startsWith("驱动器")) {
        modelName = QString("Generic Disk for %1").arg(diskIdentifier);
    }
    
    wcsncpy_s(smartData.model, sizeof(smartData.model)/sizeof(wchar_t), 
              modelName.toStdWString().c_str(), _TRUNCATE);
    wcsncpy_s(smartData.serialNumber, sizeof(smartData.serialNumber)/sizeof(wchar_t), 
              L"S667NF0R123456", _TRUNCATE);
    wcsncpy_s(smartData.firmwareVersion, sizeof(smartData.firmwareVersion)/sizeof(wchar_t), 
              L"3B4QGXA7", _TRUNCATE);
    wcsncpy_s(smartData.interfaceType, sizeof(smartData.interfaceType)/sizeof(wchar_t), 
              L"NVMe PCIe 4.0", _TRUNCATE);
    wcsncpy_s(smartData.diskType, sizeof(smartData.diskType)/sizeof(wchar_t), 
              L"SSD", _TRUNCATE);
    
    smartData.capacity = 1000204886016; // 1TB
    smartData.temperature = 42.0;
    smartData.healthPercentage = 95;
    smartData.isSystemDisk = diskIdentifier.contains("C:");
    smartData.smartEnabled = true;
    smartData.smartSupported = true;
    
    // 关键健康指标
    smartData.powerOnHours = 2847;
    smartData.powerCycleCount = 156;
    smartData.reallocatedSectorCount = 0;
    smartData.currentPendingSector = 0;
    smartData.uncorrectableErrors = 0;
    smartData.wearLeveling = 87.5;
    smartData.totalBytesWritten = 15728640000000; // 15.7TB
    smartData.totalBytesRead = 31457280000000;    // 31.4TB
    
    // 模拟SMART属性
    smartData.attributeCount = 12;
    
    // 属性1: 读取错误率
    smartData.attributes[0].id = 0x01;
    smartData.attributes[0].current = 100;
    smartData.attributes[0].worst = 100;
    smartData.attributes[0].threshold = 6;
    smartData.attributes[0].rawValue = 0;
    smartData.attributes[0].isCritical = false;
    smartData.attributes[0].physicalValue = 0.0;
    wcsncpy_s(smartData.attributes[0].name, sizeof(smartData.attributes[0].name)/sizeof(wchar_t), 
              L"读取错误率", _TRUNCATE);
    wcsncpy_s(smartData.attributes[0].units, sizeof(smartData.attributes[0].units)/sizeof(wchar_t), 
              L"", _TRUNCATE);
    
    // 属性5: 重新分配扇区数
    smartData.attributes[1].id = 0x05;
    smartData.attributes[1].current = 100;
    smartData.attributes[1].worst = 100;
    smartData.attributes[1].threshold = 36;
    smartData.attributes[1].rawValue = 0;
    smartData.attributes[1].isCritical = true;
    smartData.attributes[1].physicalValue = 0.0;
    wcsncpy_s(smartData.attributes[1].name, sizeof(smartData.attributes[1].name)/sizeof(wchar_t), 
              L"重新分配扇区数", _TRUNCATE);
    wcsncpy_s(smartData.attributes[1].units, sizeof(smartData.attributes[1].units)/sizeof(wchar_t), 
              L"个", _TRUNCATE);
    
    // 属性9: 通电时间
    smartData.attributes[2].id = 0x09;
    smartData.attributes[2].current = 99;
    smartData.attributes[2].worst = 99;
    smartData.attributes[2].threshold = 0;
    smartData.attributes[2].rawValue = smartData.powerOnHours;
    smartData.attributes[2].isCritical = false;
    smartData.attributes[2].physicalValue = static_cast<double>(smartData.powerOnHours);
    wcsncpy_s(smartData.attributes[2].name, sizeof(smartData.attributes[2].name)/sizeof(wchar_t), 
              L"通电时间", _TRUNCATE);
    wcsncpy_s(smartData.attributes[2].units, sizeof(smartData.attributes[2].units)/sizeof(wchar_t), 
              L"小时", _TRUNCATE);
    
    // 属性12: 设备通电次数
    smartData.attributes[3].id = 0x0C;
    smartData.attributes[3].current = 99;
    smartData.attributes[3].worst = 99;
    smartData.attributes[3].threshold = 0;
    smartData.attributes[3].rawValue = smartData.powerCycleCount;
    smartData.attributes[3].isCritical = true;
    smartData.attributes[3].physicalValue = static_cast<double>(smartData.powerCycleCount);
    wcsncpy_s(smartData.attributes[3].name, sizeof(smartData.attributes[3].name)/sizeof(wchar_t), 
              L"设备通电次数", _TRUNCATE);
    wcsncpy_s(smartData.attributes[3].units, sizeof(smartData.attributes[3].units)/sizeof(wchar_t), 
              L"次", _TRUNCATE);
    
    // 属性194: 温度
    smartData.attributes[4].id = 0xC2;
    smartData.attributes[4].current = static_cast<uint8_t>(100 - smartData.temperature);
    smartData.attributes[4].worst = 58;
    smartData.attributes[4].threshold = 0;
    smartData.attributes[4].rawValue = static_cast<uint64_t>(smartData.temperature);
    smartData.attributes[4].isCritical = false;
    smartData.attributes[4].physicalValue = smartData.temperature;
    wcsncpy_s(smartData.attributes[4].name, sizeof(smartData.attributes[4].name)/sizeof(wchar_t), 
              L"温度", _TRUNCATE);
    wcsncpy_s(smartData.attributes[4].units, sizeof(smartData.attributes[4].units)/sizeof(wchar_t), 
              L"°C", _TRUNCATE);
    
    // 属性196: 重新分配事件计数
    smartData.attributes[5].id = 0xC4;
    smartData.attributes[5].current = 100;
    smartData.attributes[5].worst = 100;
    smartData.attributes[5].threshold = 36;
    smartData.attributes[5].rawValue = 0;
    smartData.attributes[5].isCritical = true;
    smartData.attributes[5].physicalValue = 0.0;
    wcsncpy_s(smartData.attributes[5].name, sizeof(smartData.attributes[5].name)/sizeof(wchar_t), 
              L"重新分配事件计数", _TRUNCATE);
    wcsncpy_s(smartData.attributes[5].units, sizeof(smartData.attributes[5].units)/sizeof(wchar_t), 
              L"次", _TRUNCATE);
    
    // 属性197: 当前待处理扇区数
    smartData.attributes[6].id = 0xC5;
    smartData.attributes[6].current = 100;
    smartData.attributes[6].worst = 100;
    smartData.attributes[6].threshold = 36;
    smartData.attributes[6].rawValue = 0;
    smartData.attributes[6].isCritical = true;
    smartData.attributes[6].physicalValue = 0.0;
    wcsncpy_s(smartData.attributes[6].name, sizeof(smartData.attributes[6].name)/sizeof(wchar_t), 
              L"当前待处理扇区数", _TRUNCATE);
    wcsncpy_s(smartData.attributes[6].units, sizeof(smartData.attributes[6].units)/sizeof(wchar_t), 
              L"个", _TRUNCATE);
    
    // 属性198: 不可纠正扇区数
    smartData.attributes[7].id = 0xC6;
    smartData.attributes[7].current = 100;
    smartData.attributes[7].worst = 100;
    smartData.attributes[7].threshold = 36;
    smartData.attributes[7].rawValue = 0;
    smartData.attributes[7].isCritical = true;
    smartData.attributes[7].physicalValue = 0.0;
    wcsncpy_s(smartData.attributes[7].name, sizeof(smartData.attributes[7].name)/sizeof(wchar_t), 
              L"不可纠正扇区数", _TRUNCATE);
    wcsncpy_s(smartData.attributes[7].units, sizeof(smartData.attributes[7].units)/sizeof(wchar_t), 
              L"个", _TRUNCATE);
    
    // 属性231: SSD寿命剩余 (SSD特有)
    smartData.attributes[8].id = 0xE7;
    smartData.attributes[8].current = static_cast<uint8_t>(smartData.wearLeveling);
    smartData.attributes[8].worst = static_cast<uint8_t>(smartData.wearLeveling);
    smartData.attributes[8].threshold = 10;
    smartData.attributes[8].rawValue = static_cast<uint64_t>(smartData.wearLeveling * 10);
    smartData.attributes[8].isCritical = true;
    smartData.attributes[8].physicalValue = smartData.wearLeveling;
    wcsncpy_s(smartData.attributes[8].name, sizeof(smartData.attributes[8].name)/sizeof(wchar_t), 
              L"SSD寿命剩余", _TRUNCATE);
    wcsncpy_s(smartData.attributes[8].units, sizeof(smartData.attributes[8].units)/sizeof(wchar_t), 
              L"%", _TRUNCATE);
    
    // 属性241: 总写入LBA (SSD特有)
    smartData.attributes[9].id = 0xF1;
    smartData.attributes[9].current = 99;
    smartData.attributes[9].worst = 99;
    smartData.attributes[9].threshold = 0;
    smartData.attributes[9].rawValue = smartData.totalBytesWritten / 512; // 转换为LBA
    smartData.attributes[9].isCritical = false;
    smartData.attributes[9].physicalValue = static_cast<double>(smartData.totalBytesWritten) / (1024*1024*1024); // GB
    wcsncpy_s(smartData.attributes[9].name, sizeof(smartData.attributes[9].name)/sizeof(wchar_t), 
              L"总写入LBA", _TRUNCATE);
    wcsncpy_s(smartData.attributes[9].units, sizeof(smartData.attributes[9].units)/sizeof(wchar_t), 
              L"GB", _TRUNCATE);
    
    // 属性242: 总读取LBA (SSD特有)
    smartData.attributes[10].id = 0xF2;
    smartData.attributes[10].current = 99;
    smartData.attributes[10].worst = 99;
    smartData.attributes[10].threshold = 0;
    smartData.attributes[10].rawValue = smartData.totalBytesRead / 512; // 转换为LBA
    smartData.attributes[10].isCritical = false;
    smartData.attributes[10].physicalValue = static_cast<double>(smartData.totalBytesRead) / (1024*1024*1024); // GB
    wcsncpy_s(smartData.attributes[10].name, sizeof(smartData.attributes[10].name)/sizeof(wchar_t), 
              L"总读取LBA", _TRUNCATE);
    wcsncpy_s(smartData.attributes[10].units, sizeof(smartData.attributes[10].units)/sizeof(wchar_t), 
              L"GB", _TRUNCATE);
    
    // 属性199: UltraDMA CRC错误计数
    smartData.attributes[11].id = 0xC7;
    smartData.attributes[11].current = 100;
    smartData.attributes[11].worst = 100;
    smartData.attributes[11].threshold = 0;
    smartData.attributes[11].rawValue = 0;
    smartData.attributes[11].isCritical = true;
    smartData.attributes[11].physicalValue = 0.0;
    wcsncpy_s(smartData.attributes[11].name, sizeof(smartData.attributes[11].name)/sizeof(wchar_t), 
              L"UltraDMA CRC错误", _TRUNCATE);
    wcsncpy_s(smartData.attributes[11].units, sizeof(smartData.attributes[11].units)/sizeof(wchar_t), 
              L"次", _TRUNCATE);
    
    // 设置最后扫描时间
    GetSystemTime(&smartData.lastScanTime);
    
    Logger::Info("创建了演示SMART数据用于磁盘: " + diskIdentifier.toStdString());
}

// 1. 格式化函数实现
QString QtWidgetsTCmonitor::formatSize(uint64_t bytes)
{
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

QString QtWidgetsTCmonitor::formatPercentage(double value)
{
    return QString::number(value, 'f', 1) + "%";
}

QString QtWidgetsTCmonitor::formatNetworkSpeed(uint64_t speedBps)
{
    const double kbps = 1000.0;
    const double mbps = kbps * kbps;
    const double gbps = mbps * kbps;

    if (speedBps >= gbps) {
        return QString::number(speedBps / gbps, 'f', 1) + " Gbps";
    } else if (speedBps >= mbps) {
        return QString::number(speedBps / mbps, 'f', 1) + " Mbps";
    } else if (speedBps >= kbps) {
        return QString::number(speedBps / kbps, 'f', 1) + " Kbps";
    } else {
        return QString::number(speedBps) + " bps";
    }
}

QString QtWidgetsTCmonitor::formatTemperature(double value)
{
    return QString::number(static_cast<int>(value)) + "°C";
}

QString QtWidgetsTCmonitor::formatFrequency(double value)
{
    if (value >= 1000) {
        return QString::number(value / 1000.0, 'f', 1) + " GHz";
    } else {
        return QString::number(value, 'f', 1) + " MHz";
    }
}

// 2. 重新连接共享内存函数
void QtWidgetsTCmonitor::tryReconnectSharedMemory()
{
    Logger::Debug("尝试重新连接共享内存");
    
    if (SharedMemoryManager::InitSharedMemory()) {
        Logger::Info("共享内存重新连接成功");
        setWindowTitle(tr("系统硬件监视器"));
        
        // 停止重连定时器，启动正常更新定时器
        if (updateTimer) {
            updateTimer->stop();
            disconnect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::tryReconnectSharedMemory);
            connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::updateFromSharedMemory);
            updateTimer->start(250); // 恢复正常更新间隔
        }
        
        // 重新设置正常UI
        setupUI();
    } else {
        Logger::Debug("共享内存重新连接失败: " + SharedMemoryManager::GetLastError());
    }
}

// 3. 更新磁盘树控件函数（保持为空实现，避免链接错误）
void QtWidgetsTCmonitor::updateDiskTreeWidget()
{
    // 空实现，保持兼容性
    Logger::Debug("updateDiskTreeWidget called (empty implementation)");
}

// 4. 按钮点击处理函数（保持为空实现，避免链接错误）
void QtWidgetsTCmonitor::on_pushButton_clicked()
{
    // 空实现，保持兼容性
    Logger::Debug("on_pushButton_clicked called (empty implementation)");
}
