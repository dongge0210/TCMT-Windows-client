

#include <iostream>
#include <string>
#include <chrono>

#include <thread>
#include <algorithm>
#include "../third_party/TC/include/tc_terminal.hpp"
#include "../third_party/TC/include/tc_stream.hpp"
#include "DataUpdateManager.h"
#include "TerminalRenderer.h"
#include "DisplayFormatter.h"
#ifdef _WIN32
#include <conio.h>
#endif

int main() {
    using namespace tc;
    bool running = true;
    auto lastSize = terminal::getSize();
    int last_cols = lastSize.first;
    int last_rows = lastSize.second;
    terminal::clear();
    Printer p;
    p.hideCursor();

    DataUpdateManager dataManager(1000); // 1s update interval
    dataManager.Start();
    [[maybe_unused]] TerminalRenderer renderer;

    while (running) {
        auto size = terminal::getSize();
        int cols = size.first;
        int rows = size.second;
        if (cols != last_cols || rows != last_rows) {
            terminal::clear();
            last_cols = cols;
            last_rows = rows;
        }

        CLISystemInfo sysInfo = dataManager.GetCurrentData();
        std::string diagInfo = dataManager.GetDiagnosticInfo();
        std::vector<std::string> logs = dataManager.GetRecentDiagnosticLogs();



        std::ostringstream frame;
        int y = 1;
        auto append_full_line = [&](const std::string& content) {
            std::string blank(cols, ' ');
            frame << "\033[" << y << ";1H" << blank;
            frame << "\033[" << y << ";1H" << content;
            ++y;
        };

        append_full_line(DisplayFormatter::FormatTitle("System Monitor (TCMT-CLI)"));
        append_full_line(DisplayFormatter::FormatSection("System Info"));
        append_full_line(DisplayFormatter::FormatKeyValue("CPU Usage", std::to_string(sysInfo.cpuUsage)));
        append_full_line(DisplayFormatter::FormatKeyValue("Memory Usage", std::to_string(sysInfo.memoryUsagePercent) + "%"));
        append_full_line(DisplayFormatter::FormatKeyValue("Connection", sysInfo.connectionStatus));
        append_full_line(DisplayFormatter::FormatSection("Diagnostics"));
        append_full_line(diagInfo);

        int logCount = (std::min)(5, (int)logs.size());
        for (int i = 0; i < logCount; ++i) {
            append_full_line(logs[logs.size() - logCount + i]);
        }

        std::string resinfo = "Terminal size: " + std::to_string(cols) + " x " + std::to_string(rows);
        int rx = (cols - (int)resinfo.size()) / 2 + 1;
        frame << "\033[" << (rows - 1) << ";" << rx << "H" << resinfo;

        std::cout << frame.str() << std::flush;

#ifdef _WIN32
        if (_kbhit()) {
            char ch = (char)_getch();
            if (ch == 'q' || ch == 'Q') running = false;
        }
#else
        // 可扩展其它平台的退出逻辑
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    p.showCursor();
    dataManager.Stop();
    return 0;
}