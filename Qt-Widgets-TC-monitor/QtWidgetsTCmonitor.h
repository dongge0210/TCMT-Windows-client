// QtWidgetsTCmonitor.h
#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMessageBox>
#include "ui_QtWidgetsTCmonitor.h"  // 将在项目目录下生成

class QtWidgetsTCmonitor : public QMainWindow {
    Q_OBJECT
public:
    explicit QtWidgetsTCmonitor(QWidget* parent = nullptr);
    ~QtWidgetsTCmonitor();

private slots:
    void on_pushButton_clicked();

private:
    Ui::QtWidgetsTCmonitorClass ui;
};