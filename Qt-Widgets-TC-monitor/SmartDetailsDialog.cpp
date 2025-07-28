#include "stdafx.h"
#include "SmartDetailsDialog.h"
#include "../src/core/utils/Logger.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtCore/QStandardPaths>
#include <QTextStream>
#include <QFile>

SmartDetailsDialog::SmartDetailsDialog(const PhysicalDiskSmartData& smartData, QWidget *parent)
    : QDialog(parent), currentSmartData(smartData)
{
    setupUI();
    populateSmartData(smartData);
    
    setWindowTitle(QString("SMART Details - %1").arg(QString::fromWCharArray(smartData.model)));
    setMinimumSize(900, 700);
    resize(1100, 800);
    setModal(true);
}

SmartDetailsDialog::~SmartDetailsDialog()
{
}

void SmartDetailsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    tabWidget = new QTabWidget(this);
    
    createOverviewTab();
    createAttributesTab();
    createDiskInfoTab();
    createPerformanceTab();
    
    mainLayout->addWidget(tabWidget);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    refreshButton = new QPushButton("Refresh", this);
    refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    // 使用传统连接方式而不是新的语法
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(onRefreshClicked()));
    
    exportButton = new QPushButton("Export Report", this);
    exportButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(exportButton, SIGNAL(clicked()), this, SLOT(onExportClicked()));
    
    closeButton = new QPushButton("Close", this);
    closeButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    connect(closeButton, SIGNAL(clicked()), this, SLOT(accept()));
    
    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void SmartDetailsDialog::createOverviewTab()
{
    overviewTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(overviewTab);
    
    QGroupBox* basicInfoGroup = new QGroupBox("Basic Disk Information");
    QGridLayout* basicLayout = new QGridLayout(basicInfoGroup);
    
    basicLayout->addWidget(new QLabel("Model:"), 0, 0);
    diskModelLabel = new QLabel();
    diskModelLabel->setFont(QFont("", -1, QFont::Bold));
    basicLayout->addWidget(diskModelLabel, 0, 1);
    
    basicLayout->addWidget(new QLabel("Serial Number:"), 1, 0);
    diskSerialLabel = new QLabel();
    basicLayout->addWidget(diskSerialLabel, 1, 1);
    
    basicLayout->addWidget(new QLabel("Capacity:"), 2, 0);
    diskCapacityLabel = new QLabel();
    basicLayout->addWidget(diskCapacityLabel, 2, 1);
    
    basicLayout->addWidget(new QLabel("Type:"), 3, 0);
    diskTypeLabel = new QLabel();
    basicLayout->addWidget(diskTypeLabel, 3, 1);
    
    layout->addWidget(basicInfoGroup);
    
    QGroupBox* healthGroup = new QGroupBox("Health Status");
    QGridLayout* healthLayout = new QGridLayout(healthGroup);
    
    healthLayout->addWidget(new QLabel("Overall Health:"), 0, 0);
    healthPercentageLabel = new QLabel();
    healthPercentageLabel->setFont(QFont("", 12, QFont::Bold));
    healthLayout->addWidget(healthPercentageLabel, 0, 1);
    
    healthProgressBar = new QProgressBar();
    healthProgressBar->setRange(0, 100);
    healthProgressBar->setTextVisible(true);
    healthLayout->addWidget(healthProgressBar, 1, 0, 1, 2);
    
    healthLayout->addWidget(new QLabel("Temperature:"), 2, 0);
    temperatureLabel = new QLabel();
    healthLayout->addWidget(temperatureLabel, 2, 1);
    
    layout->addWidget(healthGroup);
    
    QGroupBox* usageGroup = new QGroupBox("Usage Statistics");
    QGridLayout* usageLayout = new QGridLayout(usageGroup);
    
    usageLayout->addWidget(new QLabel("Power On Time:"), 0, 0);
    powerOnTimeLabel = new QLabel();
    usageLayout->addWidget(powerOnTimeLabel, 0, 1);
    
    usageLayout->addWidget(new QLabel("Power Cycle Count:"), 1, 0);
    powerCycleLabel = new QLabel();
    usageLayout->addWidget(powerCycleLabel, 1, 1);
    
    layout->addWidget(usageGroup);
    layout->addStretch();
    
    tabWidget->addTab(overviewTab, "Overview");
}

void SmartDetailsDialog::createAttributesTab()
{
    attributesTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(attributesTab);
    
    attributesTree = new QTreeWidget();
    QStringList headers;
    headers << "ID" << "Attribute Name" << "Current" << "Worst" << "Threshold" << "Raw Value" << "Status";
    attributesTree->setHeaderLabels(headers);
    attributesTree->setAlternatingRowColors(true);
    attributesTree->setSortingEnabled(true);
    attributesTree->setRootIsDecorated(false);
    
    attributesTree->setColumnWidth(0, 50);
    attributesTree->setColumnWidth(1, 200);
    attributesTree->setColumnWidth(2, 80);
    attributesTree->setColumnWidth(3, 80);
    attributesTree->setColumnWidth(4, 80);
    attributesTree->setColumnWidth(5, 120);
    attributesTree->setColumnWidth(6, 80);
    
    // 使用传统连接方式
    connect(attributesTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), 
            this, SLOT(onAttributeItemClicked(QTreeWidgetItem*, int)));
    
    layout->addWidget(attributesTree);
    
    QGroupBox* detailGroup = new QGroupBox("Attribute Details");
    QVBoxLayout* detailLayout = new QVBoxLayout(detailGroup);
    
    attributeDetailText = new QTextEdit();
    attributeDetailText->setMaximumHeight(120);
    attributeDetailText->setReadOnly(true);
    detailLayout->addWidget(attributeDetailText);
    
    layout->addWidget(detailGroup);
    
    tabWidget->addTab(attributesTab, "SMART Attributes");
}

void SmartDetailsDialog::createDiskInfoTab()
{
    diskInfoTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(diskInfoTab);
    
    QGroupBox* infoGroup = new QGroupBox("Detailed Information");
    QGridLayout* infoLayout = new QGridLayout(infoGroup);
    
    int row = 0;
    
    infoLayout->addWidget(new QLabel("Firmware Version:"), row, 0);
    firmwareLabel = new QLabel();
    infoLayout->addWidget(firmwareLabel, row++, 1);
    
    infoLayout->addWidget(new QLabel("Interface Type:"), row, 0);
    interfaceLabel = new QLabel();
    infoLayout->addWidget(interfaceLabel, row++, 1);
    
    infoLayout->addWidget(new QLabel("SMART Support:"), row, 0);
    smartSupportLabel = new QLabel();
    infoLayout->addWidget(smartSupportLabel, row++, 1);
    
    infoLayout->addWidget(new QLabel("SMART Enabled:"), row, 0);
    smartEnabledLabel = new QLabel();
    infoLayout->addWidget(smartEnabledLabel, row++, 1);
    
    infoLayout->addWidget(new QLabel("System Disk:"), row, 0);
    systemDiskLabel = new QLabel();
    infoLayout->addWidget(systemDiskLabel, row++, 1);
    
    infoLayout->addWidget(new QLabel("Last Scan:"), row, 0);
    lastScanLabel = new QLabel();
    infoLayout->addWidget(lastScanLabel, row++, 1);
    
    layout->addWidget(infoGroup);
    layout->addStretch();
    
    tabWidget->addTab(diskInfoTab, "Disk Information");
}

void SmartDetailsDialog::createPerformanceTab()
{
    performanceTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(performanceTab);
    
    QGroupBox* ioGroup = new QGroupBox("I/O Statistics");
    QGridLayout* ioLayout = new QGridLayout(ioGroup);
    
    ioLayout->addWidget(new QLabel("Total Bytes Written:"), 0, 0);
    bytesWrittenLabel = new QLabel();
    ioLayout->addWidget(bytesWrittenLabel, 0, 1);
    
    ioLayout->addWidget(new QLabel("Total Bytes Read:"), 1, 0);
    bytesReadLabel = new QLabel();
    ioLayout->addWidget(bytesReadLabel, 1, 1);
    
    layout->addWidget(ioGroup);
    
    QGroupBox* errorGroup = new QGroupBox("Error Statistics");
    QGridLayout* errorLayout = new QGridLayout(errorGroup);
    
    errorLayout->addWidget(new QLabel("Reallocated Sectors:"), 0, 0);
    reallocatedSectorsLabel = new QLabel();
    errorLayout->addWidget(reallocatedSectorsLabel, 0, 1);
    
    errorLayout->addWidget(new QLabel("Pending Sectors:"), 1, 0);
    pendingSectorsLabel = new QLabel();
    errorLayout->addWidget(pendingSectorsLabel, 1, 1);
    
    errorLayout->addWidget(new QLabel("Uncorrectable Errors:"), 2, 0);
    uncorrectableErrorsLabel = new QLabel();
    errorLayout->addWidget(uncorrectableErrorsLabel, 2, 1);
    
    layout->addWidget(errorGroup);
    
    QGroupBox* ssdGroup = new QGroupBox("SSD Specific Metrics");
    QGridLayout* ssdLayout = new QGridLayout(ssdGroup);
    
    ssdLayout->addWidget(new QLabel("Wear Leveling:"), 0, 0);
    wearLevelingLabel = new QLabel();
    ssdLayout->addWidget(wearLevelingLabel, 0, 1);
    
    layout->addWidget(ssdGroup);
    layout->addStretch();
    
    tabWidget->addTab(performanceTab, "Performance Metrics");
}

void SmartDetailsDialog::populateSmartData(const PhysicalDiskSmartData& smartData)
{
    updateHealthOverview(smartData);
    updateAttributesTable(smartData);
    updateDiskInfo(smartData);
    updatePerformanceMetrics(smartData);
}

void SmartDetailsDialog::updateHealthOverview(const PhysicalDiskSmartData& smartData)
{
    diskModelLabel->setText(QString::fromWCharArray(smartData.model));
    diskSerialLabel->setText(QString::fromWCharArray(smartData.serialNumber));
    diskCapacityLabel->setText(formatBytes(smartData.capacity));
    diskTypeLabel->setText(QString::fromWCharArray(smartData.diskType));
    
    QString healthText = QString("%1%").arg(smartData.healthPercentage);
    QString healthStatus = getHealthStatus(smartData.healthPercentage);
    healthPercentageLabel->setText(QString("%1 (%2)").arg(healthText, healthStatus));
    healthPercentageLabel->setStyleSheet(QString("color: %1").arg(getHealthColor(smartData.healthPercentage).name()));
    
    healthProgressBar->setValue(smartData.healthPercentage);
    healthProgressBar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(getHealthColor(smartData.healthPercentage).name()));
    
    if (smartData.temperature > 0) {
        temperatureLabel->setText(QString("%1C").arg(static_cast<int>(smartData.temperature)));
        if (smartData.temperature > 60) {
            temperatureLabel->setStyleSheet("color: red; font-weight: bold;");
        } else if (smartData.temperature > 45) {
            temperatureLabel->setStyleSheet("color: orange;");
        } else {
            temperatureLabel->setStyleSheet("color: green;");
        }
    } else {
        temperatureLabel->setText("Unknown");
        temperatureLabel->setStyleSheet("");
    }
    
    powerOnTimeLabel->setText(formatTime(smartData.powerOnHours));
    powerCycleLabel->setText(QString::number(smartData.powerCycleCount));
}

void SmartDetailsDialog::updateAttributesTable(const PhysicalDiskSmartData& smartData)
{
    attributesTree->clear();
    
    for (int i = 0; i < smartData.attributeCount && i < 32; ++i) {
        const SmartAttributeData& attr = smartData.attributes[i];
        
        QTreeWidgetItem* item = new QTreeWidgetItem();
        
        item->setText(0, QString("0x%1").arg(attr.id, 2, 16, QChar('0')).toUpper());
        
        QString attrName = QString::fromWCharArray(attr.name);
        if (attrName.isEmpty()) {
            attrName = SmartAttributeInfo::getAttributeName(attr.id);
        }
        item->setText(1, attrName);
        
        item->setText(2, QString::number(attr.current));
        item->setText(3, QString::number(attr.worst));
        
        if (attr.threshold > 0) {
            item->setText(4, QString::number(attr.threshold));
        } else {
            item->setText(4, "---");
        }
        
        item->setText(5, SmartAttributeInfo::formatRawValue(attr.id, attr.rawValue));
        
        QString status;
        QColor statusColor = Qt::black;
        
        if (attr.threshold > 0 && attr.current <= attr.threshold) {
            status = "Failed";
            statusColor = Qt::red;
        } else if (attr.current < attr.worst * 1.1) {
            status = "Warning";
            statusColor = QColor(255, 165, 0);
        } else {
            status = "OK";
            statusColor = Qt::green;
        }
        
        item->setText(6, status);
        item->setForeground(6, statusColor);
        
        if (attr.isCritical || SmartAttributeInfo::isCriticalAttribute(attr.id)) {
            QFont font = item->font(1);
            font.setBold(true);
            item->setFont(1, font);
        }
        
        item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<const void*>(&attr)));
        
        attributesTree->addTopLevelItem(item);
    }
    
    attributesTree->sortItems(0, Qt::AscendingOrder);
}

void SmartDetailsDialog::updateDiskInfo(const PhysicalDiskSmartData& smartData)
{
    firmwareLabel->setText(QString::fromWCharArray(smartData.firmwareVersion));
    interfaceLabel->setText(QString::fromWCharArray(smartData.interfaceType));
    
    smartSupportLabel->setText(smartData.smartSupported ? "Yes" : "No");
    smartSupportLabel->setStyleSheet(smartData.smartSupported ? "color: green;" : "color: red;");
    
    smartEnabledLabel->setText(smartData.smartEnabled ? "Yes" : "No");
    smartEnabledLabel->setStyleSheet(smartData.smartEnabled ? "color: green;" : "color: red;");
    
    systemDiskLabel->setText(smartData.isSystemDisk ? "Yes" : "No");
    systemDiskLabel->setStyleSheet(smartData.isSystemDisk ? "color: blue; font-weight: bold;" : "");
    
    QDateTime scanTime = QDateTime::fromSecsSinceEpoch(0);
    lastScanLabel->setText(scanTime.toString("yyyy-MM-dd hh:mm:ss"));
}

void SmartDetailsDialog::updatePerformanceMetrics(const PhysicalDiskSmartData& smartData)
{
    bytesWrittenLabel->setText(formatBytes(smartData.totalBytesWritten));
    bytesReadLabel->setText(formatBytes(smartData.totalBytesRead));
    
    reallocatedSectorsLabel->setText(QString::number(smartData.reallocatedSectorCount));
    if (smartData.reallocatedSectorCount > 0) {
        reallocatedSectorsLabel->setStyleSheet("color: orange; font-weight: bold;");
    }
    
    pendingSectorsLabel->setText(QString::number(smartData.currentPendingSector));
    if (smartData.currentPendingSector > 0) {
        pendingSectorsLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    uncorrectableErrorsLabel->setText(QString::number(smartData.uncorrectableErrors));
    if (smartData.uncorrectableErrors > 0) {
        uncorrectableErrorsLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    if (smartData.wearLeveling >= 0) {
        wearLevelingLabel->setText(QString("%1%").arg(smartData.wearLeveling, 0, 'f', 1));
        if (smartData.wearLeveling < 10) {
            wearLevelingLabel->setStyleSheet("color: red; font-weight: bold;");
        } else if (smartData.wearLeveling < 30) {
            wearLevelingLabel->setStyleSheet("color: orange;");
        } else {
            wearLevelingLabel->setStyleSheet("color: green;");
        }
    } else {
        wearLevelingLabel->setText("N/A");
    }
}

void SmartDetailsDialog::onAttributeItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) return;
    
    const SmartAttributeData* attr = static_cast<const SmartAttributeData*>(item->data(0, Qt::UserRole).value<const void*>());
    
    if (attr) {
        QString detailText = QString("Attribute ID: 0x%1 (%2)\nAttribute Name: %3\nDescription: %4\n\nCurrent Value: %5\nWorst Value: %6\nThreshold: %7\nRaw Value: %8\nPhysical Value: %9 %10\n\nCritical Attribute: %11")
            .arg(attr->id, 2, 16, QChar('0')).toUpper()
            .arg(attr->id)
            .arg(QString::fromWCharArray(attr->name))
            .arg(SmartAttributeInfo::getAttributeDescription(attr->id))
            .arg(attr->current)
            .arg(attr->worst)
            .arg(attr->threshold > 0 ? QString::number(attr->threshold) : "---")
            .arg(attr->rawValue)
            .arg(attr->physicalValue, 0, 'f', 2)
            .arg(QString::fromWCharArray(attr->units))
            .arg(attr->isCritical ? "Yes" : "No");
        
        attributeDetailText->setPlainText(detailText);
    }
}

void SmartDetailsDialog::onRefreshClicked()
{
    QMessageBox::information(this, "Refresh", "SMART data refresh functionality is not implemented yet");
}

void SmartDetailsDialog::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export SMART Report", QString("SMART_Report_%1_%2.txt").arg(QString::fromWCharArray(currentSmartData.model).replace(" ", "_")).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")), "Text Files (*.txt);;All Files (*.*)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot create file: " + fileName);
        return;
    }
    
    QTextStream out(&file);
    
    out << "SMART Disk Health Report\n";
    out << "==========================================\n\n";
    
    out << "Basic Disk Information:\n";
    out << "Model: " << QString::fromWCharArray(currentSmartData.model) << "\n";
    out << "Serial Number: " << QString::fromWCharArray(currentSmartData.serialNumber) << "\n";
    out << "Firmware Version: " << QString::fromWCharArray(currentSmartData.firmwareVersion) << "\n";
    out << "Capacity: " << formatBytes(currentSmartData.capacity) << "\n";
    out << "Type: " << QString::fromWCharArray(currentSmartData.diskType) << "\n";
    out << "Interface: " << QString::fromWCharArray(currentSmartData.interfaceType) << "\n\n";
    
    out << "Health Status:\n";
    out << "Overall Health: " << currentSmartData.healthPercentage << "%\n";
    out << "Temperature: " << currentSmartData.temperature << " C\n";
    out << "Power On Time: " << formatTime(currentSmartData.powerOnHours) << "\n";
    out << "Power Cycle Count: " << currentSmartData.powerCycleCount << "\n\n";
    
    out << "SMART Attributes Details:\n";
    out << "ID\tAttribute Name\t\t\tCurrent\tWorst\tThreshold\tRaw Value\t\tStatus\n";
    out << "------------------------------------------------------------------------\n";
    
    for (int i = 0; i < currentSmartData.attributeCount && i < 32; ++i) {
        const SmartAttributeData& attr = currentSmartData.attributes[i];
        QString attrName = QString::fromWCharArray(attr.name);
        if (attrName.isEmpty()) {
            attrName = SmartAttributeInfo::getAttributeName(attr.id);
        }
        
        out << QString("0x%1").arg(attr.id, 2, 16, QChar('0')).toUpper() << "\t";
        out << attrName.leftJustified(24) << "\t";
        out << QString::number(attr.current) << "\t";
        out << QString::number(attr.worst) << "\t";
        out << (attr.threshold > 0 ? QString::number(attr.threshold) : "---") << "\t";
        out << QString::number(attr.rawValue) << "\t\t";
        
        QString status = "OK";
        if (attr.threshold > 0 && attr.current <= attr.threshold) {
            status = "Failed";
        } else if (attr.current < attr.worst * 1.1) {
            status = "Warning";
        }
        out << status << "\n";
    }
    
    out << "\nReport Generation Time: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    
    file.close();
    
    QMessageBox::information(this, "Export Successful", "SMART report saved to: " + fileName);
}

QString SmartDetailsDialog::formatBytes(uint64_t bytes)
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

QString SmartDetailsDialog::formatTime(uint64_t hours)
{
    if (hours == 0) return "0 hours";
    
    uint64_t days = hours / 24;
    uint64_t remainingHours = hours % 24;
    
    if (days > 365) {
        uint64_t years = days / 365;
        days = days % 365;
        return QString("%1 years %2 days %3 hours").arg(years).arg(days).arg(remainingHours);
    } else if (days > 0) {
        return QString("%1 days %2 hours").arg(days).arg(remainingHours);
    } else {
        return QString("%1 hours").arg(hours);
    }
}

QString SmartDetailsDialog::getHealthStatus(uint8_t healthPercentage)
{
    if (healthPercentage >= 90) return "Excellent";
    else if (healthPercentage >= 75) return "Good";
    else if (healthPercentage >= 50) return "Fair";
    else if (healthPercentage >= 25) return "Poor";
    else return "Critical";
}

QColor SmartDetailsDialog::getHealthColor(uint8_t healthPercentage)
{
    if (healthPercentage >= 75) return Qt::green;
    else if (healthPercentage >= 50) return QColor(255, 165, 0);
    else if (healthPercentage >= 25) return QColor(255, 140, 0);
    else return Qt::red;
}

QString SmartAttributeInfo::getAttributeName(uint8_t id)
{
    static QMap<uint8_t, QString> attributeNames = {
        {0x01, "Read Error Rate"},
        {0x02, "Throughput Performance"},
        {0x03, "Spin-Up Time"},
        {0x04, "Start/Stop Count"},
        {0x05, "Reallocated Sectors Count"},
        {0x06, "Read Channel Margin"},
        {0x07, "Seek Error Rate"},
        {0x08, "Seek Time Performance"},
        {0x09, "Power-On Hours"},
        {0x0A, "Spin Retry Count"},
        {0x0B, "Recalibration Retries"},
        {0x0C, "Power Cycle Count"},
        {0x0D, "Soft Read Error Rate"},
        {0xC2, "Temperature"},
        {0xC4, "Reallocation Event Count"},
        {0xC5, "Current Pending Sector Count"},
        {0xC6, "Uncorrectable Sector Count"},
        {0xC7, "UltraDMA CRC Error Count"}
    };
    
    return attributeNames.value(id, QString("Unknown Attribute (0x%1)").arg(id, 2, 16, QChar('0')).toUpper());
}

QString SmartAttributeInfo::getAttributeDescription(uint8_t id)
{
    static QMap<uint8_t, QString> descriptions = {
        {0x01, "Rate of hardware read errors that occurred when reading data from a disk surface"},
        {0x05, "Count of reallocated sectors. When the hard drive finds a read/write/verification error, it marks this sector as reallocated"},
        {0x09, "Count of hours in power-on state"},
        {0x0C, "Count of hard disk power-on cycles"},
        {0xC2, "Indicates the device temperature"},
        {0xC4, "Count of operations of data transfer (remapping) from possibly bad sectors to reserved special area"},
        {0xC5, "Count of unstable sectors (waiting for remapping)"},
        {0xC6, "Count of uncorrectable errors when reading/writing a sector"}
    };
    
    return descriptions.value(id, "Detailed description for this attribute is not available");
}

bool SmartAttributeInfo::isCriticalAttribute(uint8_t id)
{
    static QSet<uint8_t> criticalAttributes = {
        0x05, 0x0A, 0x0C, 0xC4, 0xC5, 0xC6, 0xC7
    };
    
    return criticalAttributes.contains(id);
}

QString SmartAttributeInfo::formatRawValue(uint8_t id, uint64_t rawValue)
{
    switch (id) {
        case 0x09:
            return QString("%1 hours").arg(rawValue);
        case 0x0C:
            return QString("%1 cycles").arg(rawValue);
        case 0xC2:
            return QString("%1C").arg(rawValue & 0xFF);
        case 0xF1:
        case 0xF2:
            return QString("%1 LBAs").arg(rawValue);
        default:
            return QString::number(rawValue);
    }
}