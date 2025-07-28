#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollArea>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include "../src/core/DataStruct/DataStruct.h"

// Helper class for SMART attribute information
class SmartAttributeInfo
{
public:
    static QString getAttributeName(uint8_t id);
    static QString getAttributeDescription(uint8_t id);
    static bool isCriticalAttribute(uint8_t id);
    static QString formatRawValue(uint8_t id, uint64_t rawValue);
};

class SmartDetailsDialog : public QDialog
{
public:
    explicit SmartDetailsDialog(const PhysicalDiskSmartData& smartData, QWidget *parent = nullptr);
    ~SmartDetailsDialog();

public slots:
    void onRefreshClicked();
    void onExportClicked();
    void onAttributeItemClicked(QTreeWidgetItem *item, int column);

private:
    void setupUI();
    void createOverviewTab();
    void createAttributesTab();
    void createDiskInfoTab();
    void createPerformanceTab();
    void populateSmartData(const PhysicalDiskSmartData& smartData);
    void updateHealthOverview(const PhysicalDiskSmartData& smartData);
    void updateAttributesTable(const PhysicalDiskSmartData& smartData);
    void updateDiskInfo(const PhysicalDiskSmartData& smartData);
    void updatePerformanceMetrics(const PhysicalDiskSmartData& smartData);
    
    QString formatBytes(uint64_t bytes);
    QString formatTime(uint64_t hours);
    QString getHealthStatus(uint8_t healthPercentage);
    QColor getHealthColor(uint8_t healthPercentage);
    
    // UI components
    QTabWidget* tabWidget;
    
    // Overview tab
    QWidget* overviewTab;
    QLabel* diskModelLabel;
    QLabel* diskSerialLabel;
    QLabel* diskCapacityLabel;
    QLabel* diskTypeLabel;
    QLabel* healthPercentageLabel;
    QProgressBar* healthProgressBar;
    QLabel* temperatureLabel;
    QLabel* powerOnTimeLabel;
    QLabel* powerCycleLabel;
    
    // SMART attributes tab
    QWidget* attributesTab;
    QTreeWidget* attributesTree;
    QTextEdit* attributeDetailText;
    
    // Disk info tab
    QWidget* diskInfoTab;
    QLabel* firmwareLabel;
    QLabel* interfaceLabel;
    QLabel* smartSupportLabel;
    QLabel* smartEnabledLabel;
    QLabel* systemDiskLabel;
    QLabel* lastScanLabel;
    
    // Performance metrics tab
    QWidget* performanceTab;
    QLabel* bytesWrittenLabel;
    QLabel* bytesReadLabel;
    QLabel* reallocatedSectorsLabel;
    QLabel* pendingSectorsLabel;
    QLabel* uncorrectableErrorsLabel;
    QLabel* wearLevelingLabel;
    
    // Buttons
    QPushButton* refreshButton;
    QPushButton* exportButton;
    QPushButton* closeButton;
    
    // Data
    PhysicalDiskSmartData currentSmartData;
};