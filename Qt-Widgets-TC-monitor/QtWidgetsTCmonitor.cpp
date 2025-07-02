#include "stdafx.h"
#include "QtWidgetsTCmonitor.h"
#include "ui_QtWidgetsTCmonitor.h"  // Auto-generated UI file
#include <windows.h>
#include "../src/core/DataStruct/SharedMemoryManager.h"

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtGui/QPainter>
#include <QTimer>
#include <QMainWindow>
#include <QObject>
#include <QtWidgets>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <map>       // For std::map
#include <string>    // For std::string
#include <QLabel>    // For QLabel
#include <queue>
#include <sstream>
#include <iomanip>
#include <QString>
#include <vector>

QtWidgetsTCmonitor::QtWidgetsTCmonitor(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();

    // Set window properties
    setWindowTitle(tr("系统硬件监视器"));
    resize(800, 600);

    // Initialize shared memory access
    if (!SharedMemoryManager::InitSharedMemory()) {
        QMessageBox::critical(this, tr("错误"), 
            tr("无法初始化共享内存，请确保 project1.exe 正在运行。\n错误信息: ") + 
            QString::fromStdString(SharedMemoryManager::GetLastError()));
    }

    // Set up update timer - connect to shared memory reader instead of just charts
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::updateFromSharedMemory);
    connect(updateTimer, &QTimer::timeout, this, &QtWidgetsTCmonitor::updateCharts);
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
        if (temp.first == "CPU Package" || temp.first == "CPU Temperature" || temp.first == "CPU Average Core") {
            cpuTemp = temp.second;
            cpuFound = true;
            infoLabels["cpuTemp"]->setText(formatTemperature(cpuTemp));
        }
        else if (temp.first.find("GPU Core") != std::string::npos) {
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
        QString diskLabel = QString("%1: %2").arg(disk.letter).arg(tr("驱动器"));
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

void QtWidgetsTCmonitor::updateCharts()
{
    // Update CPU temperature chart
    cpuTempSeries->clear();
    int pointIndex = 0;
    std::queue<float> cpuTempCopy = cpuTempHistory;
    while (!cpuTempCopy.empty()) {
        cpuTempSeries->append(pointIndex, cpuTempCopy.front());
        cpuTempCopy.pop();
        pointIndex++;
    }

    // Update GPU temperature chart
    gpuTempSeries->clear();
    pointIndex = 0;
    std::queue<float> gpuTempCopy = gpuTempHistory;
    while (!gpuTempCopy.empty()) {
        gpuTempSeries->append(pointIndex, gpuTempCopy.front());
        gpuTempCopy.pop();
        pointIndex++;
    }
}

void QtWidgetsTCmonitor::on_pushButton_clicked()
{
    QMessageBox::information(this, tr("系统监控"), tr("正在监控系统硬件信息"));
}

QString QtWidgetsTCmonitor::formatSize(uint64_t bytes)
{
    constexpr double KB = 1024.0;
    constexpr double MB = KB * KB;
    constexpr double GB = MB * KB;
    constexpr double TB = GB * KB;

    QString result;
    if (bytes >= TB) {
        result = QString("%1 TB").arg(bytes / TB, 0, 'f', 2);
    }
    else if (bytes >= GB) {
        result = QString("%1 GB").arg(bytes / GB, 0, 'f', 2);
    }
    else if (bytes >= MB) {
        result = QString("%1 MB").arg(bytes / MB, 0, 'f', 2);
    }
    else if (bytes >= KB) {
        result = QString("%1 KB").arg(bytes / KB, 0, 'f', 2);
    }
    else {
        result = QString("%1 B").arg(bytes);
    }

    return result;
}

QString QtWidgetsTCmonitor::formatPercentage(double value)
{
    // 改进百分比格式化，处理小数精度
    if (value < 0.1 && value > 0.0) {
        return QString("< 0.1%");
    } else if (value >= 99.95) {
        return QString("100%");
    } else {
        return QString("%1%").arg(value, 0, 'f', 1);
    }
}

QString QtWidgetsTCmonitor::formatTemperature(double value)
{
    return QString("%1°C").arg(static_cast<int>(value));
}

QString QtWidgetsTCmonitor::formatFrequency(double value)
{
    if (value >= 1000) {
        return QString("%1 GHz").arg(value / 1000.0, 0, 'f', 2);
    }
    else {
        return QString("%1 MHz").arg(value, 0, 'f', 2);
    }
}

// Ensure data is read from shared memory and updates the UI
void QtWidgetsTCmonitor::updateFromSharedMemory() {
    SharedMemoryBlock* pBuffer = SharedMemoryManager::GetBuffer();
    if (!pBuffer) {
        // Try to reinitialize
        if (!SharedMemoryManager::InitSharedMemory()) {
            return;
        }
        pBuffer = SharedMemoryManager::GetBuffer();
        if (!pBuffer) return;
    }

    if (!TryEnterCriticalSection(&pBuffer->lock)) {
        // If we can't get the lock immediately, skip this update
        return;
    }

    try {
        // Update CPU information with improved formatting
        infoLabels["cpuName"]->setText(safeFromWCharArray(pBuffer->cpuName, 128));
        infoLabels["physicalCores"]->setText(QString::number(pBuffer->physicalCores));
        infoLabels["logicalCores"]->setText(QString::number(pBuffer->logicalCores));
        
        // 改进CPU使用率显示
        QString cpuUsageText = formatPercentage(pBuffer->cpuUsage);
        infoLabels["cpuUsage"]->setText(cpuUsageText);
        
        // 添加调试信息
        static int updateCounter = 0;
        if (++updateCounter % 10 == 0) {
            Logger::Info("Qt界面CPU使用率显示: " + cpuUsageText.toStdString());
        }
        
        infoLabels["performanceCores"]->setText(QString::number(pBuffer->performanceCores));
        infoLabels["efficiencyCores"]->setText(QString::number(pBuffer->efficiencyCores));
        infoLabels["hyperThreading"]->setText(pBuffer->hyperThreading ? tr("已启用") : tr("未启用"));
        infoLabels["virtualization"]->setText(pBuffer->virtualization ? tr("已启用") : tr("未启用"));

        // Update GPU information with virtual GPU indication
        if (pBuffer->gpuCount > 0) {
            QString gpuName = safeFromWCharArray(pBuffer->gpus[0].name, 128);
            QString gpuBrand = safeFromWCharArray(pBuffer->gpus[0].brand, 64);
            
            // 添加虚拟显卡标识
            if (pBuffer->gpus[0].isVirtual) {
                gpuName += tr(" (虚拟)");
                gpuBrand += tr(" (虚拟)");
            }
            
            infoLabels["gpuName"]->setText(gpuName);
            infoLabels["gpuBrand"]->setText(gpuBrand);
            infoLabels["gpuMemory"]->setText(formatSize(pBuffer->gpus[0].memory));
            infoLabels["gpuCoreFreq"]->setText(formatFrequency(pBuffer->gpus[0].coreClock));
        } else {
            infoLabels["gpuName"]->setText(tr("未检测到GPU"));
            infoLabels["gpuBrand"]->setText(tr("未知"));
            infoLabels["gpuMemory"]->setText(tr("0 B"));
            infoLabels["gpuCoreFreq"]->setText(tr("0 MHz"));
        }

        // Update temperature data and history for charts
        float cpuTemp = 0;
        float gpuTemp = 0;
        bool cpuFound = false;
        bool gpuFound = false;

        for (int i = 0; i < pBuffer->tempCount && i < 10; ++i) {
            QString sensorName = safeFromWCharArray(pBuffer->temperatures[i].sensorName, 64);
            double temperature = pBuffer->temperatures[i].temperature;
            
            if (sensorName.contains("CPU", Qt::CaseInsensitive) || 
                sensorName.contains("Package", Qt::CaseInsensitive)) {
                cpuTemp = static_cast<float>(temperature);
                cpuFound = true;
                infoLabels["cpuTemp"]->setText(formatTemperature(temperature));
            }
            else if (sensorName.contains("GPU", Qt::CaseInsensitive) || 
                     sensorName.contains("Graphics", Qt::CaseInsensitive)) {
                gpuTemp = static_cast<float>(temperature);
                gpuFound = true;
                infoLabels["gpuTemp"]->setText(formatTemperature(temperature));
            }
        }

        // Update temperature history for charts
        if (cpuFound) {
            cpuTempHistory.push(cpuTemp);
            if (cpuTempHistory.size() > MAX_DATA_POINTS) {
                cpuTempHistory.pop();
            }
        } else {
            infoLabels["cpuTemp"]->setText(tr("无数据"));
        }

        if (gpuFound) {
            gpuTempHistory.push(gpuTemp);
            if (gpuTempHistory.size() > MAX_DATA_POINTS) {
                gpuTempHistory.pop();
            }
        } else {
            infoLabels["gpuTemp"]->setText(tr("无数据"));
        }

        // Update disk information
        updateDiskInfoFromSharedMemory(pBuffer);
        
    }
    catch (const std::exception& e) {
        Logger::Error("Qt界面更新异常: " + std::string(e.what()));
        infoLabels["cpuName"]->setText(tr("读取错误"));
    }
    catch (...) {
        Logger::Error("Qt界面更新未知异常");
        infoLabels["cpuName"]->setText(tr("未知错误"));
    }
    
    LeaveCriticalSection(&pBuffer->lock);
}

// Add helper method for disk info updates
void QtWidgetsTCmonitor::updateDiskInfoFromSharedMemory(SharedMemoryBlock* pBuffer) {
    if (!pBuffer) {
        return;
    }
    if (pBuffer->diskCount < 0 || pBuffer->diskCount > 8) {
        return;
    }

    // Get or create disk container
    QLayout* currentLayout = diskGroupBox->layout();
    QWidget* diskContainer = nullptr;

    if (currentLayout) {
        for (int i = 0; i < currentLayout->count(); ++i) {
            QWidget* widget = currentLayout->itemAt(i)->widget();
            if (widget) {
                diskContainer = widget;
                break;
            }
        }
    }

    if (!diskContainer) {
        diskContainer = new QWidget(diskGroupBox);
        static_cast<QVBoxLayout*>(currentLayout)->addWidget(diskContainer);
    }

    // Clear existing layout
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

    // Create new layout and add disk info
    QVBoxLayout* diskLayout = new QVBoxLayout(diskContainer);

    for (int i = 0; i < pBuffer->diskCount && i < 8; ++i) {
        const auto& disk = pBuffer->disks[i];

        // Use safe string conversion
        QString label = safeFromWCharArray(disk.label, 128);
        QString fileSystem = safeFromWCharArray(disk.fileSystem, 32);

        QString diskLabel = QString("%1: %2").arg(QChar(disk.letter)).arg(tr("驱动器"));
        if (!label.isEmpty()) {
            diskLabel += QString(" (%1)").arg(label);
        }

        QGroupBox* diskBox = new QGroupBox(diskLabel);
        QGridLayout* diskInfoLayout = new QGridLayout(diskBox);

        int row = 0;
        if (!fileSystem.isEmpty()) {
            diskInfoLayout->addWidget(new QLabel(tr("文件系统:")), row, 0);
            diskInfoLayout->addWidget(new QLabel(fileSystem), row++, 1);
        }

        diskInfoLayout->addWidget(new QLabel(tr("总容量:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatSize(disk.totalSize)), row++, 1);

        diskInfoLayout->addWidget(new QLabel(tr("已用空间:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatSize(disk.usedSpace)), row++, 1);

        diskInfoLayout->addWidget(new QLabel(tr("可用空间:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatSize(disk.freeSpace)), row++, 1);

        double usagePercent = disk.totalSize > 0 ?
            (static_cast<double>(disk.usedSpace) / disk.totalSize * 100.0) : 0.0;
        diskInfoLayout->addWidget(new QLabel(tr("使用率:")), row, 0);
        diskInfoLayout->addWidget(new QLabel(formatPercentage(usagePercent)), row++, 1);

        diskLayout->addWidget(diskBox);
    }

    diskLayout->addStretch();
}
