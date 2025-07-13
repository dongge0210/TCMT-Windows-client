#include "B_UI/ConsoleUI.h"
#include "core/utils/Logger.h"
#include "core/DataStruct/SharedMemoryManager.h"
#include "BackgroundMonitor.h"
#include <iostream>
#include <thread>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

// 显示启动选择菜单
int ShowStartupMenu() {
    std::cout << "==================================================" << std::endl;
    std::cout << "        Windows 系统监控器 启动选择菜单" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    std::cout << "请选择运行模式:" << std::endl;
    std::cout << "1. 控制台UI界面 (推荐)" << std::endl;
    std::cout << "2. 后台监控模式" << std::endl;
    std::cout << "3. 同时运行 (UI + 后台监控)" << std::endl;
    std::cout << "4. 退出" << std::endl;
    std::cout << std::endl;
    std::cout << "请输入选择 (1-4): ";
    
    int choice;
    std::cin >> choice;
    return choice;
}

// UI模式运行函数
int RunUIMode() {
    try {
        // 设置控制台输出为UTF-8
        _setmode(_fileno(stdout), _O_U8TEXT);
        
        // 初始化Logger（UI模式下减少控制台输出）
        Logger::EnableConsoleOutput(false); // 禁用控制台输出，避免干扰UI
        Logger::Initialize("system_monitor_ui.log");
        Logger::SetLogLevel(LOG_INFO); // UI模式下使用INFO等级
        Logger::Info("启动UI模式");
        
        // 检查共享内存是否可用
        if (!SharedMemoryManager::GetBuffer()) {
            std::cout << "警告: 后台监控程序未运行，某些功能可能不可用。" << std::endl;
            std::cout << "建议先启动后台监控程序或选择'同时运行'模式。" << std::endl;
            std::cout << "是否继续? (y/n): ";
            char confirm;
            std::cin >> confirm;
            if (confirm != 'y' && confirm != 'Y') {
                return 0;
            }
        }
        
        // 创建并运行UI
        ConsoleUI ui;
        if (!ui.Initialize()) {
            std::cerr << "UI初始化失败" << std::endl;
            return 1;
        }
        
        ui.Run();
        ui.Shutdown();
        
        Logger::Info("UI模式正常退出");
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "UI模式发生错误: " << e.what() << std::endl;
        Logger::Error("UI模式发生错误: " + std::string(e.what()));
        return 1;
    }
}

// 混合模式运行函数
int RunMixedMode(int argc, char* argv[]) {
    try {
        Logger::Info("启动混合模式 (UI + 后台监控)");
        
        // 启动后台监控线程
        std::thread backgroundThread([argc, argv]() {
            try {
                BackgroundMonitor(argc, argv);
            }
            catch (const std::exception& e) {
                Logger::Error("后台监控线程错误: " + std::string(e.what()));
            }
        });
        
        // 等待一段时间让后台监控初始化
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 运行UI
        int uiResult = RunUIMode();
        
        // UI退出后，后台监控继续运行
        if (backgroundThread.joinable()) {
            std::cout << std::endl;
            std::cout << "UI已退出，后台监控继续运行..." << std::endl;
            std::cout << "按 Ctrl+C 停止后台监控" << std::endl;
            backgroundThread.join();
        }
        
        return uiResult;
    }
    catch (const std::exception& e) {
        std::cerr << "混合模式发生错误: " << e.what() << std::endl;
        Logger::Error("混合模式发生错误: " + std::string(e.what()));
        return 1;
    }
}

// 新的main函数 - 启动选择器
int main(int argc, char* argv[]) {
    // 检查命令行参数
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--ui") {
            return RunUIMode();
        } else if (arg == "--background") {
            return BackgroundMonitor(argc, argv);
        } else if (arg == "--mixed") {
            return RunMixedMode(argc, argv);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  --ui         只运行UI界面" << std::endl;
            std::cout << "  --background 只运行后台监控" << std::endl;
            std::cout << "  --mixed      同时运行UI和后台监控" << std::endl;
            std::cout << "  --help, -h   显示此帮助信息" << std::endl;
            std::cout << std::endl;
            std::cout << "不带参数运行将显示启动选择菜单" << std::endl;
            return 0;
        }
    }
    
    // 显示启动选择菜单
    int choice = ShowStartupMenu();
    
    switch (choice) {
        case 1:
            return RunUIMode();
        case 2:
            return BackgroundMonitor(argc, argv);
        case 3:
            return RunMixedMode(argc, argv);
        case 4:
            std::cout << "再见!" << std::endl;
            return 0;
        default:
            std::cout << "无效选择，退出程序" << std::endl;
            return 1;
    }
}
