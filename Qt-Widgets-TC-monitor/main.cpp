#include "stdafx.h"
#include "QtWidgetsTCmonitor.h"
#include <QtWidgets/QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QMessageBox>
#include <windows.h>
#include "../src/core/utils/Logger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("系统硬件监视器");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("TC Monitor");

    // 设置应用程序样式
    app.setStyle(QStyleFactory::create("Fusion"));

    // 设置深色主题
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);

    // 初始化日志系统 - 添加异常处理
    try {
        Logger::Initialize("qt_monitor.log");
        Logger::EnableConsoleOutput(false); // QT-UI不需要控制台输出
        Logger::SetLogLevel(LOG_INFO);
        
        Logger::Info("QT-UI 系统监控器启动");
    }
    catch (const std::exception& e) {
        QMessageBox::warning(nullptr, "日志系统警告", 
            QString("日志系统初始化失败: %1\n程序将继续运行但不记录日志。").arg(e.what()));
    }

    try {
        // 创建主窗口 - 添加异常处理
        QtWidgetsTCmonitor window;
        
        // 设置窗口图标（如果有的话）
        // window.setWindowIcon(QIcon(":/icons/monitor.ico"));
        
        // 显示窗口
        window.show();
        
        if (Logger::IsInitialized()) {
            Logger::Info("QT-UI 主窗口已显示");
        }
        
        // 运行应用程序事件循环
        int result = app.exec();
        
        if (Logger::IsInitialized()) {
            Logger::Info("QT-UI 正常退出，返回码: " + std::to_string(result));
        }
        return result;
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("QT-UI 启动时发生致命错误: %1").arg(e.what());
        
        if (Logger::IsInitialized()) {
            Logger::Error(errorMsg.toStdString());
        }
        
        QMessageBox::critical(nullptr, "启动错误", 
            errorMsg + "\n\n请检查：\n1. 主程序是否正在运行\n2. 共享内存是否可用\n3. Qt库是否正确安装");
        return -1;
    }
    catch (...) {
        QString errorMsg = "QT-UI 启动时发生未知错误";
        
        if (Logger::IsInitialized()) {
            Logger::Error(errorMsg.toStdString());
        }
        
        QMessageBox::critical(nullptr, "启动错误", 
            errorMsg + "\n\n请尝试：\n1. 重新启动主程序\n2. 以管理员权限运行\n3. 重新编译项目");
        return -1;
    }
}
