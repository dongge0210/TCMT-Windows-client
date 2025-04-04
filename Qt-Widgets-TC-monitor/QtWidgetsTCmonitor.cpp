#include "stdafx.h"
#include "QtWidgetsTCmonitor.h"

QtWidgetsTCmonitor::QtWidgetsTCmonitor(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    // C++20 features can be used here if needed
}

QtWidgetsTCmonitor::~QtWidgetsTCmonitor() = default;

void QtWidgetsTCmonitor::on_pushButton_clicked()
{
    QMessageBox::information(this, "Test", "test");
}
