#include "TuiApp.h"
#include <ncurses.h>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace tcmt {

// ============================================================================
// TuiApp
// ============================================================================

TuiApp::TuiApp() {}

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
}

bool TuiApp::IsRunning() const {
    return running_.load();
}

void TuiApp::UpdateData(const TuiData& data) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    data_ = data;
}

LogBuffer& TuiApp::GetLogBuffer() {
    return logBuffer_;
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
        bar += (i < filled) ? "█" : "░";
    }
    return bar;
}

std::string TuiApp::TrimRight(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen);
}

void TuiApp::DrawHeader(WINDOW* win) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)rows;

    wattron(win, COLOR_PAIR(1) | A_BOLD);
    std::string title = "TCMT Monitor - macOS";
    int x = (cols - static_cast<int>(title.size())) / 2;
    mvwprintw(win, 0, std::max(0, x), "%s", title.c_str());

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);

    wattroff(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 0, cols - 10, "%s", buf);
}

void TuiApp::DrawCpuPanel(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)rows; (void)cols;

    int y = 0;
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, 1, "CPU");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    // Name (truncate if needed)
    std::string name = TrimRight(data.cpuName, cols - 12);
    mvwprintw(win, y++, 2, "%s", name.c_str());

    // Usage bar
    int barWidth = std::min(cols - 20, 30);
    mvwprintw(win, y, 2, "Usage:");
    wattron(win, COLOR_PAIR(6));
    mvwprintw(win, y, 9, "%s", FormatBar(data.cpuUsage, barWidth).c_str());
    wattroff(win, COLOR_PAIR(6));
    mvwprintw(win, y, 10 + barWidth, "%5.1f%%", data.cpuUsage);
    y++;

    // Cores
    if (data.performanceCores > 0 || data.efficiencyCores > 0) {
        mvwprintw(win, y++, 2, "P-cores: %d (%.0f MHz)  E-cores: %d (%.0f MHz)",
                  data.performanceCores, data.pCoreFreq,
                  data.efficiencyCores, data.eCoreFreq);
    } else {
        mvwprintw(win, y++, 2, "Cores: %d physical, %d logical",
                  data.physicalCores, data.physicalCores);
    }

    // Temperature
    if (data.cpuTemp > 0) {
        int tempColor = (data.cpuTemp > 80) ? 4 : (data.cpuTemp > 60) ? 3 : 2;
        wattron(win, COLOR_PAIR(tempColor));
        mvwprintw(win, y++, 2, "Temp: %.0f°C", data.cpuTemp);
        wattroff(win, COLOR_PAIR(tempColor));
    }
}

void TuiApp::DrawMemoryPanel(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)rows; (void)cols;

    int y = 0;
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, 1, "Memory");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    double usagePct = (data.totalMemory > 0)
                      ? 100.0 * data.usedMemory / data.totalMemory : 0;

    int barWidth = std::min(cols - 22, 30);
    mvwprintw(win, y, 2, "Used:");
    wattron(win, COLOR_PAIR(6));
    mvwprintw(win, y, 8, "%s", FormatBar(usagePct, barWidth).c_str());
    wattroff(win, COLOR_PAIR(6));
    mvwprintw(win, y, 9 + barWidth, "%s / %s",
              FormatSize(data.usedMemory).c_str(),
              FormatSize(data.totalMemory).c_str());
    y++;

    mvwprintw(win, y++, 2, "Avail: %s", FormatSize(data.availableMemory).c_str());
}

void TuiApp::DrawGpuPanel(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)rows; (void)cols;

    int y = 0;
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, 1, "GPU");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    if (!data.gpuName.empty()) {
        std::string name = TrimRight(data.gpuName, cols - 4);
        mvwprintw(win, y++, 2, "%s", name.c_str());
    } else {
        mvwprintw(win, y++, 2, "(no GPU detected)");
        return;
    }

    if (data.gpuMemory > 0) {
        mvwprintw(win, y++, 2, "VRAM: %s", FormatSize(data.gpuMemory).c_str());
    }

    if (data.gpuUsage > 0) {
        int barWidth = std::min(cols - 20, 30);
        mvwprintw(win, y, 2, "Use:");
        wattron(win, COLOR_PAIR(6));
        mvwprintw(win, y, 7, "%s", FormatBar(data.gpuUsage, barWidth).c_str());
        wattroff(win, COLOR_PAIR(6));
        mvwprintw(win, y, 8 + barWidth, "%5.1f%%", data.gpuUsage);
        y++;
    }

    if (data.gpuTemp > 0) {
        int tempColor = (data.gpuTemp > 80) ? 4 : (data.gpuTemp > 60) ? 3 : 2;
        wattron(win, COLOR_PAIR(tempColor));
        mvwprintw(win, y++, 2, "Temp: %.0f°C", data.gpuTemp);
        wattroff(win, COLOR_PAIR(tempColor));
    }
}

void TuiApp::DrawDiskPanel(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)cols;

    int y = 0;
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, 1, "Disks");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    int maxRows = rows - 1;
    for (size_t i = 0; i < data.disks.size() && y < maxRows; ++i) {
        const auto& d = data.disks[i];
        double usagePct = (d.totalSize > 0)
                          ? 100.0 * d.usedSpace / d.totalSize : 0;

        std::string label = TrimRight(d.label.empty() ? "Untitled" : d.label, 20);
        mvwprintw(win, y, 2, "%-20s", label.c_str());

        wattron(win, COLOR_PAIR(6));
        wprintw(win, "%s", FormatBar(usagePct, 15).c_str());
        wattroff(win, COLOR_PAIR(6));

        wprintw(win, " %5.1f%% %s",
                usagePct,
                FormatSize(d.usedSpace).c_str());
        y++;
    }
}

void TuiApp::DrawNetworkPanel(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)cols;

    int y = 0;
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, 1, "Network");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    int maxRows = rows - 1;
    for (size_t i = 0; i < data.adapters.size() && y < maxRows; ++i) {
        const auto& n = data.adapters[i];
        if (n.ip.empty()) continue; // skip disconnected

        std::string name = TrimRight(n.name, 12);
        std::string type = TrimRight(n.type, 10);
        std::string ip = TrimRight(n.ip, 16);

        mvwprintw(win, y, 2, "%-12s %-10s %-16s",
                  name.c_str(), type.c_str(), ip.c_str());

        if (n.speed > 0) {
            wprintw(win, " %s", FormatSpeed(n.speed).c_str());
        }
        y++;
    }
}

void TuiApp::DrawTempPanel(WINDOW* win, const TuiData& data) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)cols;

    int y = 0;
    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, y++, 1, "Temps");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    int maxRows = rows - 1;
    for (size_t i = 0; i < data.temperatures.size() && y < maxRows; ++i) {
        const auto& [name, temp] = data.temperatures[i];

        int tempColor = (temp > 80) ? 4 : (temp > 60) ? 3 : 2;
        std::string sensor = TrimRight(name, 16);

        wattron(win, COLOR_PAIR(tempColor));
        mvwprintw(win, y++, 2, "%-16s %5.1f°C", sensor.c_str(), temp);
        wattroff(win, COLOR_PAIR(tempColor));
    }
}

void TuiApp::DrawLogPanel(WINDOW* win) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)cols;

    wattron(win, COLOR_PAIR(5) | A_BOLD);
    mvwprintw(win, 0, 1, "Log");
    wattroff(win, COLOR_PAIR(5) | A_BOLD);

    auto lines = logBuffer_.GetRecent(rows - 1);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];

        // Determine color based on log level
        int color = 2; // default green
        if (line.find("[ERROR]") != std::string::npos) color = 4;
        else if (line.find("[WARN]") != std::string::npos) color = 3;
        else if (line.find("[DEBUG]") != std::string::npos) color = 6;

        wattron(win, COLOR_PAIR(color));
        std::string trimmed = TrimRight(line, cols - 2);
        mvwprintw(win, i + 1, 1, "%s", trimmed.c_str());
        wattroff(win, COLOR_PAIR(color));
    }
}

void TuiApp::Run() {
    // Initialize ncurses
    initscr();
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
        // Handle resize
        int newRows, newCols;
        getmaxyx(stdscr, newRows, newCols);
        if (newRows != termRows_ || newCols != termCols_) {
            termRows_ = newRows;
            termCols_ = newCols;
            resizeterm(termRows_, termCols_);
        }

        // Handle input
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) { // q or ESC
            running_ = false;
            break;
        }

        // Clear
        erase();

        // Calculate panel layout
        int headerH = 2;
        int logH = std::max(5, termRows_ / 4);
        int mainH = termRows_ - headerH - logH - 2; // -2 for separators

        int leftW = termCols_ / 2;
        int rightW = termCols_ - leftW;

        int leftPanelH = mainH / 3;
        int rightPanelH = mainH - leftPanelH;

        // Draw header
        WINDOW* header = newwin(headerH, termCols_, 0, 0);
        DrawHeader(header);
        wrefresh(header);
        delwin(header);

        // Draw separator
        mvhline(headerH, 0, ACS_HLINE, termCols_);

        // Get current data
        TuiData data;
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            data = data_;
        }

        // Left column: CPU + Memory
        int cpuH = leftPanelH;
        int memH = leftPanelH;
        WINDOW* cpuWin = newwin(cpuH, leftW, headerH + 1, 0);
        DrawCpuPanel(cpuWin, data);
        wrefresh(cpuWin);
        delwin(cpuWin);

        WINDOW* memWin = newwin(memH, leftW, headerH + 1 + cpuH, 0);
        DrawMemoryPanel(memWin, data);
        wrefresh(memWin);
        delwin(memWin);

        // Right column: GPU + Disk/Network/Temp
        int gpuH = rightPanelH / 2;
        int miscH = rightPanelH - gpuH;

        WINDOW* gpuWin = newwin(gpuH, rightW, headerH + 1, leftW);
        DrawGpuPanel(gpuWin, data);
        wrefresh(gpuWin);
        delwin(gpuWin);

        // Misc panel (Disk/Network/Temp stacked)
        WINDOW* miscWin = newwin(miscH, rightW, headerH + 1 + gpuH, leftW);
        // Split misc into 3 rows
        int miscRows, miscCols;
        getmaxyx(miscWin, miscRows, miscCols);
        int rowH = miscRows / 3;

        WINDOW* diskWin = subwin(miscWin, rowH, miscCols, headerH + 1 + gpuH, leftW);
        DrawDiskPanel(diskWin, data);
        wrefresh(diskWin);
        delwin(diskWin);

        WINDOW* netWin = subwin(miscWin, rowH, miscCols, headerH + 1 + gpuH + rowH, leftW);
        DrawNetworkPanel(netWin, data);
        wrefresh(netWin);
        delwin(netWin);

        WINDOW* tempWin = subwin(miscWin, miscRows - 2*rowH, miscCols, headerH + 1 + gpuH + 2*rowH, leftW);
        DrawTempPanel(tempWin, data);
        wrefresh(tempWin);
        delwin(tempWin);

        delwin(miscWin);

        // Draw separator before log
        mvhline(termRows_ - logH - 1, 0, ACS_HLINE, termCols_);

        // Draw log panel
        WINDOW* logWin = newwin(logH, termCols_, termRows_ - logH, 0);
        DrawLogPanel(logWin);
        wrefresh(logWin);
        delwin(logWin);

        // Refresh and sleep
        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    endwin();
}

} // namespace tcmt
