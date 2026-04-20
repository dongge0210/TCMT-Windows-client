#include "TuiApp.h"
#include <ncurses.h>
#include <locale>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace tcmt {

// ============================================================================
// TuiApp
// ============================================================================

TuiApp::TuiApp() {
    logBuf_ = &defaultBuffer_;
}

TuiApp::~TuiApp() {
    Stop();
}

void TuiApp::Start() {
    if (running_.load()) return;
    running_ = true;
    thread_ = std::thread(&TuiApp::Run, this);
}

void TuiApp::Stop() {
    if (!running_.load()) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    // Safety: ensure terminal is restored even if Run() didn't reach endwin()
    SafeEndwin();
    std::fflush(stdout);
}

bool TuiApp::IsRunning() const {
    return running_.load();
}

void TuiApp::UpdateData(const TuiData& data) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    data_ = data;
}

LogBuffer& TuiApp::GetLogBuffer() {
    return defaultBuffer_;
}

void TuiApp::SetLogBuffer(LogBuffer* buf) {
    logBuf_ = buf ? buf : &defaultBuffer_;
}

void TuiApp::InitColors() {
    if (!has_colors()) return;
    start_color();
    use_default_colors();

    init_pair(1, COLOR_CYAN, -1);    // Header
    init_pair(2, COLOR_GREEN, -1);  // Normal
    init_pair(3, COLOR_YELLOW, -1); // Warning
    init_pair(4, COLOR_RED, -1);   // Error/Critical
    init_pair(5, COLOR_WHITE, -1); // Label
    init_pair(6, COLOR_BLUE, -1);  // Bar
}

std::string TuiApp::FormatSize(uint64_t bytes) {
    const double GB = 1024.0 * 1024.0 * 1024.0;
    const double MB = 1024.0 * 1024.0;
    const double KB = 1024.0;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= (uint64_t)(10 * GB)) ss << (bytes / GB) << " GB";
    else if (bytes >= (uint64_t)GB) ss << (bytes / GB) << " GB";
    else if (bytes >= (uint64_t)MB) ss << (bytes / MB) << " MB";
    else if (bytes >= (uint64_t)KB) ss << (bytes / KB) << " KB";
    else ss << bytes << " B";
    return ss.str();
}

std::string TuiApp::FormatSpeed(uint64_t bps) {
    const double GB = 1000.0 * 1000.0 * 1000.0;
    const double MB = 1000.0 * 1000.0;
    const double KB = 1000.0;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0);
    if (bps >= (uint64_t)GB) ss << (bps / GB) << " Gbps";
    else if (bps >= (uint64_t)MB) ss << (bps / MB) << " Mbps";
    else if (bps >= (uint64_t)KB) ss << (bps / KB) << " Kbps";
    else ss << bps << " bps";
    return ss.str();
}

std::string TuiApp::FormatBar(double pct, int width) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int filled = static_cast<int>(pct * width / 100.0);
    std::string bar;
    for (int i = 0; i < width; ++i) {
        bar += (i < filled) ? '=' : '-';
    }
    return bar;
}

std::string TuiApp::TrimRight(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen);
}

void TuiApp::DrawHeader(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)rows;
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    std::string title = "TCMT Monitor - macOS  " + data.timestamp;
    int x = (cols - static_cast<int>(title.size())) / 2;
    mvwprintw(win, 0, std::max(0, x), "%s", title.c_str());
    wattroff(win, COLOR_PAIR(1) | A_BOLD);
}

void TuiApp::DrawCpuPanel(WINDOW* win, const TuiData& data, int y, int x0, int maxW) {
    int bw = std::min(maxW - 14, 20);
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, x0, "CPU");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    std::string name = TrimRight(data.cpuName, maxW - 4);
    mvwprintw(win, y++, x0 + 2, "%s", name.c_str());

    mvwprintw(win, y, x0 + 2, "Use:");
    wattron(win, COLOR_PAIR(6));
    mvwprintw(win, y++, x0 + 8, "%s", FormatBar(data.cpuUsage, bw).c_str());
    wattroff(win, COLOR_PAIR(6));
    mvwprintw(win, y - 1, x0 + 9 + bw, "%.1f%%", data.cpuUsage);

    if (data.performanceCores > 0 || data.efficiencyCores > 0) {
        if (data.pCoreFreq > 0 || data.eCoreFreq > 0) {
            mvwprintw(win, y++, x0 + 2, "P:%d(%.0fM) E:%d(%.0fM)",
                      data.performanceCores, data.pCoreFreq,
                      data.efficiencyCores, data.eCoreFreq);
        } else {
            mvwprintw(win, y++, x0 + 2, "P:%d E:%d",
                      data.performanceCores, data.efficiencyCores);
        }
    } else {
        mvwprintw(win, y++, x0 + 2, "Cores: %d", data.physicalCores);
    }

    if (data.cpuTemp > 0) {
        int tc = (data.cpuTemp > 80) ? 4 : (data.cpuTemp > 60) ? 3 : 2;
        wattron(win, COLOR_PAIR(tc));
        mvwprintw(win, y++, x0 + 2, "Temp: %.0f C", data.cpuTemp);
        wattroff(win, COLOR_PAIR(tc));
    }
}

void TuiApp::DrawMemoryPanel(WINDOW* win, const TuiData& data, int y, int x0, int maxW) {
    int bw = std::min(maxW - 16, 20);
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, x0, "RAM");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    double upct = (data.totalMemory > 0) ? 100.0 * data.usedMemory / data.totalMemory : 0;
    mvwprintw(win, y, x0 + 2, "Used:");
    wattron(win, COLOR_PAIR(6));
    mvwprintw(win, y++, x0 + 8, "%s", FormatBar(upct, bw).c_str());
    wattroff(win, COLOR_PAIR(6));
    mvwprintw(win, y - 1, x0 + 9 + bw, "%s / %s",
              FormatSize(data.usedMemory).c_str(),
              FormatSize(data.totalMemory).c_str());
    mvwprintw(win, y++, x0 + 2, "Avail: %s", FormatSize(data.availableMemory).c_str());
    if (data.compressedMemory > 0) {
        mvwprintw(win, y++, x0 + 2, "Compressed: %s", FormatSize(data.compressedMemory).c_str());
    }
}

void TuiApp::DrawGpuPanel(WINDOW* win, const TuiData& data, int y, int x0, int maxW) {
    int bw = std::min(maxW - 16, 20);
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, x0, "GPU");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    std::string name = TrimRight(data.gpuName, maxW - 4);
    mvwprintw(win, y++, x0 + 2, "%s", name.c_str());

    mvwprintw(win, y, x0 + 2, "Use:");
    wattron(win, COLOR_PAIR(6));
    mvwprintw(win, y++, x0 + 8, "%s", FormatBar(data.gpuUsage, bw).c_str());
    wattroff(win, COLOR_PAIR(6));
    mvwprintw(win, y - 1, x0 + 9 + bw, "%.1f%%", data.gpuUsage);

    if (data.gpuTemp > 0) {
        int tc = (data.gpuTemp > 80) ? 4 : (data.gpuTemp > 60) ? 3 : 2;
        wattron(win, COLOR_PAIR(tc));
        mvwprintw(win, y++, x0 + 2, "Temp: %.0f C", data.gpuTemp);
        wattroff(win, COLOR_PAIR(tc));
    }
}

void TuiApp::DrawDiskPanel(WINDOW* win, const TuiData& data, int y, int x0, int maxW) {
    int bw = std::min(maxW - 22, 14);
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, x0, "Disks");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    for (const auto& d : data.disks) {
        double upct = (d.totalSize > 0) ? 100.0 * d.usedSpace / d.totalSize : 0;
        std::string label = TrimRight(d.label.empty() ? "Untitled" : d.label, 14);
        mvwprintw(win, y, x0 + 2, "%-14s", label.c_str());
        wattron(win, COLOR_PAIR(6));
        wprintw(win, "%s", FormatBar(upct, bw).c_str());
        wattroff(win, COLOR_PAIR(6));
        wprintw(win, "%.0f%% %s", upct, FormatSize(d.usedSpace).c_str());
        y++;
    }
}

void TuiApp::DrawNetworkPanel(WINDOW* win, const TuiData& data, int y, int x0, int maxW) {
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, x0, "Network");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    for (const auto& n : data.adapters) {
        if (n.ip.empty()) continue;
        std::string name = TrimRight(n.name, 10);
        std::string ip = TrimRight(n.ip, maxW - 14);
        mvwprintw(win, y++, x0 + 2, "%-10s %s", name.c_str(), ip.c_str());
        if (n.speed > 0) {
            mvwprintw(win, y++, x0 + 4, "%s", FormatSpeed(n.speed).c_str());
        }
    }
}

void TuiApp::DrawTempPanel(WINDOW* win, const TuiData& data, int y, int x0, int maxW) {
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, x0, "Temps");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    for (const auto& [name, temp] : data.temperatures) {
        int tc = (temp > 80) ? 4 : (temp > 60) ? 3 : 2;
        std::string sensor = TrimRight(name, maxW - 12);
        wattron(win, COLOR_PAIR(tc));
        mvwprintw(win, y++, x0 + 2, "%-14s %.0f C", sensor.c_str(), temp);
        wattroff(win, COLOR_PAIR(tc));
    }
}

void TuiApp::Run() {
    // Set locale for UTF-8 support
    setlocale(LC_ALL, "");

    // Initialize ncurses
    initscr();
    cursesActive_ = true;
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    InitColors();

    // Get terminal size
    getmaxyx(stdscr, termRows_, termCols_);

    // Main loop
    while (running_.load()) {
        // Get current terminal size
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        // Handle input (non-blocking)
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) {
            running_ = false;
            break;
        }

        // Handle resize
        if (rows != termRows_ || cols != termCols_) {
            termRows_ = rows;
            termCols_ = cols;
        }

        if (rows < 10 || cols < 40) {
            mvprintw(0, 0, "Terminal too small (min 40x10). Current: %dx%d", cols, rows);
            refresh();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // Get current data
        TuiData data;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            data = data_;
        }

        // Clear screen
        erase();

        // === Layout calculation ===
        int y = 0;
        int leftW = cols / 2;
        int rightW = cols - leftW;

        // --- Header (2 lines) ---
        DrawHeader(stdscr, data);
        y += 2;
        mvwhline(stdscr, y, 0, '-', cols);
        y++;

        // --- Main panels: CPU+Memory (left), GPU+Disk+Net+Temp (right) ---
        int mainTop = y;
        y++;

        // Left: CPU (fixed 6 lines)
        DrawCpuPanel(stdscr, data, mainTop, 1, leftW - 1);
        y = mainTop + 7;
        mvwhline(stdscr, y, 0, '-', leftW);
        y++;

        // Left: Memory (fixed 4 lines)
        DrawMemoryPanel(stdscr, data, y, 1, leftW - 1);
        y += 4;
        mvwhline(stdscr, y, 0, '-', leftW);
        y++;

        // Right: GPU (fixed 5 lines)
        DrawGpuPanel(stdscr, data, mainTop, leftW + 1, rightW - 1);
        int rightY = mainTop + 6;
        mvwhline(stdscr, rightY, leftW, '-', rightW);
        rightY++;

        // Right: Disk (remaining space / 3)
        DrawDiskPanel(stdscr, data, rightY, leftW + 1, rightW - 1);
        int diskLines = std::min(static_cast<int>(data.disks.size() + 1), rows / 6);
        rightY += diskLines;
        mvwhline(stdscr, rightY, leftW, '-', rightW);
        rightY++;

        // Right: Network
        DrawNetworkPanel(stdscr, data, rightY, leftW + 1, rightW - 1);
        int netLines = std::min(static_cast<int>(data.adapters.size() + 1), rows / 8);
        rightY += netLines;
        mvwhline(stdscr, rightY, leftW, '-', rightW);
        rightY++;

        // Right: Temperature
        DrawTempPanel(stdscr, data, rightY, leftW + 1, rightW - 1);

        // --- Log panel (bottom) ---
        int logTop = rows - rows / 4 - 1;
        if (logTop < y + 1) logTop = y + 1;
        mvwhline(stdscr, logTop, 0, '-', cols);
        logTop++;

        // Draw log panel directly using stdscr
        wattron(stdscr, COLOR_PAIR(5) | A_BOLD);
        mvwprintw(stdscr, logTop, 1, "Log");
        wattroff(stdscr, COLOR_PAIR(5) | A_BOLD);

        auto logLines = logBuf_->GetRecent(rows - logTop - 2);
        for (size_t i = 0; i < logLines.size() && static_cast<int>(i + logTop + 1) < rows - 1; ++i) {
            const std::string& line = logLines[i];
            int color = 2;
            if (line.find("[ERROR]") != std::string::npos) color = 4;
            else if (line.find("[WARN]") != std::string::npos) color = 3;
            else if (line.find("[DEBUG]") != std::string::npos) color = 6;

            wattron(stdscr, COLOR_PAIR(color));
            std::string trimmed = TrimRight(line, cols - 2);
            mvwprintw(stdscr, logTop + 1 + i, 1, "%s", trimmed.c_str());
            wattroff(stdscr, COLOR_PAIR(color));
        }

        // Border around entire screen
        // ASCII border
        for (int c = 0; c < cols; c++) {
            mvaddch(0, c, '-');
            mvaddch(rows-1, c, '-');
        }
        for (int r = 0; r < rows; r++) {
            mvaddch(r, 0, '|');
            mvaddch(r, cols-1, '|');
        }
        mvaddch(0, 0, '+');
        mvaddch(0, cols-1, '+');
        mvaddch(rows-1, 0, '+');
        mvaddch(rows-1, cols-1, '+');

        // Left/right separator line
        for (int r = 2; r < logTop; r++) {
            mvwvline(stdscr, r, leftW, '|', 1);
        }

        // Refresh
        refresh();

        // Short sleep with periodic check for responsive exit
        for (int i = 0; i < 10 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // Cleanup
    SafeEndwin();
}

void TuiApp::SafeEndwin() {
    bool expected = true;
    if (cursesActive_.compare_exchange_strong(expected, false)) {
        endwin();
    }
}

} // namespace tcmt
