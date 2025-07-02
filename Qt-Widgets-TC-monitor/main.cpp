#include "stdafx.h"  // 放在最前面
#include "QtWidgetsTCmonitor.h"
#include "../src/core/DataStruct/SharedMemoryManager.h"
#include "../src/core/utils/Logger.h"
#include <QtWidgets/QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Initialize logger
    Logger::Initialize("qt_monitor.log");
    Logger::EnableConsoleOutput(true);
    Logger::Info("Qt Monitor application starting");

    // Create and show main window
    QtWidgetsTCmonitor window;
    window.show();

    Logger::Info("Qt Monitor application started successfully");
    
    int result = app.exec();
    
    // Cleanup
    SharedMemoryManager::CleanupSharedMemory();
    Logger::Info("Qt Monitor application exiting");
    
    return result;
}
