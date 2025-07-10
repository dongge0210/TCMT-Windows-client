#include "stdafx.h"  // 放在最前面
#include "QtWidgetsTCmonitor.h"
#include "../src/core/DataStruct/SharedMemoryManager.h"
#include "../src/core/utils/Logger.h"
#include <QtWidgets/QApplication>
#include <QMessageBox>

#include <windows.h>
#include <shellapi.h>

// 判断当前进程是否以管理员身份运行
bool IsRunAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

int main(int argc, char *argv[])
{
    // 自动提权：如果不是管理员，则尝试以管理员身份重启自身
    if (!IsRunAsAdmin()) {
        wchar_t szPath[MAX_PATH] = {0};
        GetModuleFileNameW(NULL, szPath, MAX_PATH);

        // 拼接命令行参数
        std::wstring cmdLine;
        for (int i = 1; i < argc; ++i) {
            cmdLine += L" \"";
            int len = MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, NULL, 0);
            if (len > 1) {
                std::wstring warg(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, &warg[0], len);
                warg.pop_back(); // 去掉多余的\0
                cmdLine += warg;
            }
            cmdLine += L"\"";
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.lpParameters = cmdLine.empty() ? NULL : cmdLine.c_str();
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteExW(&sei)) {
            // 启动成功，退出当前进程
            return 0;
        } else {
            QMessageBox::critical(nullptr, "权限不足", "自动提权失败，请右键以管理员身份运行。", QMessageBox::Ok);
            return 1;
        }
    }

    QApplication app(argc, argv);

    // Initialize logger
    Logger::Initialize("qt_monitor.log");
    Logger::EnableConsoleOutput(true);
    Logger::Info("Qt Monitor应用程序启动中");

    // Create and show main window
    QtWidgetsTCmonitor window;
    window.show();

    Logger::Info("Qt Monitor应用程序启动成功");

    int result = app.exec();

    // Cleanup
    SharedMemoryManager::CleanupSharedMemory();
    Logger::Info("Qt Monitor应用程序退出中");

    return result;
}
