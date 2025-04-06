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

// Include SystemInfo structure definition
#include "../src/core/ui/QtDisplayBridge.h"

// Forward declaration of UI namespace
namespace Ui {
class QtWidgetsTCmonitorClass;
}

// Use the Qt Charts namespace
QT_CHARTS_USE_NAMESPACE

// Define the maximum number of data points for charts
#define MAX_DATA_POINTS 60

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

private:
    // UI setup functions
    void setupUI();
    void createCpuSection();
    void createMemorySection();
    void createGpuSection();
    void createTemperatureSection();
    void createDiskSection();

    // Formatting helper functions
    QString formatSize(uint64_t bytes);
    QString formatPercentage(double value);
    QString formatTemperature(double value);
    QString formatFrequency(double value);

    // UI components
    Ui::QtWidgetsTCmonitorClass *ui;
    QTimer *updateTimer;

    // Main UI containers
    QGroupBox *cpuGroupBox;
    QGroupBox *memoryGroupBox;
    QGroupBox *gpuGroupBox;
    QGroupBox *temperatureGroupBox;
    QGroupBox *diskGroupBox;

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
};

