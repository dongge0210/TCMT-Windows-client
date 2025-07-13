#include "ConsoleUI.h"
#include "../core/utils/Logger.h"
#include "../core/DataStruct/SharedMemoryManager.h"
#include "../core/utils/WinUtils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <conio.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <queue>

// Global thread-safe output mutex
std::mutex g_uiConsoleMutex;

// Global thread-safe output functions
void SafeUIOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    
    // 使用Windows API而不是std::cout避免缓冲区问题
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
    }
}

void SafeUIOutput(const std::string& message, int color) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalColor = csbi.wAttributes;
        
        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
        
        DWORD written;
        WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
        
        SetConsoleTextAttribute(hConsole, originalColor);
    }
}

void SafeTerminalOutput(const std::string& message) {
    SafeUIOutput(message);
}

void SafeTerminalColorOutput(const std::string& message, const std::string& color) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        std::string output = color + message + "\033[0m";
        DWORD written;
        WriteConsoleA(hConsole, output.c_str(), static_cast<DWORD>(output.length()), &written, NULL);
    }
}

// Convert function implementation
SystemInfo ConsoleUI::ConvertFromSharedMemory(const SharedMemoryBlock* buffer) {
    SystemInfo sysInfo;
    
    if (!buffer) return sysInfo;
    
    // 基本信息
    try {
        sysInfo.cpuName = WinUtils::WstringToString(buffer->cpuName);
        sysInfo.physicalCores = buffer->physicalCores;
        sysInfo.logicalCores = buffer->logicalCores;
        sysInfo.cpuUsage = buffer->cpuUsage;
        sysInfo.performanceCores = buffer->performanceCores;
        sysInfo.efficiencyCores = buffer->efficiencyCores;
        sysInfo.performanceCoreFreq = buffer->pCoreFreq;
        sysInfo.efficiencyCoreFreq = buffer->eCoreFreq;
        sysInfo.hyperThreading = buffer->hyperThreading;
        sysInfo.virtualization = buffer->virtualization;
        
        // 内存信息
        sysInfo.totalMemory = buffer->totalMemory;
        sysInfo.usedMemory = buffer->usedMemory;
        sysInfo.availableMemory = buffer->availableMemory;
        
        // GPU信息
        if (buffer->gpuCount > 0) {
            sysInfo.gpuName = WinUtils::WstringToString(buffer->gpus[0].name);
            sysInfo.gpuBrand = WinUtils::WstringToString(buffer->gpus[0].brand);
            sysInfo.gpuMemory = buffer->gpus[0].memory;
            sysInfo.gpuCoreFreq = buffer->gpus[0].coreClock;
            sysInfo.gpuIsVirtual = buffer->gpus[0].isVirtual;
        }
        
        // 温度信息
        for (int i = 0; i < buffer->tempCount && i < 10; ++i) {
            std::string sensorName = WinUtils::WstringToString(buffer->temperatures[i].sensorName);
            sysInfo.temperatures.push_back({sensorName, buffer->temperatures[i].temperature});
        }
        
        // 磁盘信息
        for (int i = 0; i < buffer->diskCount && i < 8; ++i) {
            DiskData disk;
            disk.letter = buffer->disks[i].letter;
            disk.label = WinUtils::WstringToString(buffer->disks[i].label);
            disk.fileSystem = WinUtils::WstringToString(buffer->disks[i].fileSystem);
            disk.totalSize = buffer->disks[i].totalSize;
            disk.usedSpace = buffer->disks[i].usedSpace;
            disk.freeSpace = buffer->disks[i].freeSpace;
            sysInfo.disks.push_back(disk);
        }
        
        sysInfo.lastUpdate = buffer->lastUpdate;
    }
    catch (const std::exception& e) {
        Logger::Error("转换共享内存数据时发生错误: " + std::string(e.what()));
    }
    
    return sysInfo;
}

// ============================================================================
// ProgressBar implementation
// ============================================================================
std::string ProgressBar::Render() const {
    double percentage = GetPercentage();
    int filledWidth = static_cast<int>((percentage / 100.0) * m_width);
    
    std::stringstream ss;
    if (!m_label.empty()) {
        ss << m_label << " ";
    }
    
    ss << "[";
    ss << m_color;
    for (int i = 0; i < filledWidth; ++i) {
        ss << "=";
    }
    ss << "\033[90m";  // BRIGHT_BLACK
    for (int i = filledWidth; i < m_width; ++i) {
        ss << "-";
    }
    ss << "\033[0m] ";  // RESET
    ss << std::fixed << std::setprecision(1) << percentage << "%";
    
    return ss.str();
}

// ============================================================================
// MiniChart implementation
// ============================================================================
void MiniChart::AddValue(double value) {
    m_data.push(value);
    if (static_cast<int>(m_data.size()) > m_maxPoints) {
        m_data.pop();
    }
    
    // Update min/max values
    if (!m_data.empty()) {
        std::queue<double> temp = m_data;
        m_minValue = temp.front();
        m_maxValue = temp.front();
        
        while (!temp.empty()) {
            double val = temp.front();
            temp.pop();
            if (val < m_minValue) m_minValue = val;
            if (val > m_maxValue) m_maxValue = val;
        }
        
        if (m_maxValue == m_minValue) m_maxValue = m_minValue + 1.0;
    }
}

std::vector<std::string> MiniChart::Render() const {
    std::vector<std::string> result(m_height);
    
    if (m_data.empty()) {
        for (int i = 0; i < m_height; ++i) {
            result[i] = std::string(m_width, ' ');
        }
        return result;
    }
    
    // Convert queue to vector for index access
    std::vector<double> values;
    std::queue<double> temp = m_data;
    while (!temp.empty()) {
        values.push_back(temp.front());
        temp.pop();
    }
    
    // Create chart
    for (int y = 0; y < m_height; ++y) {
        std::string line;
        for (int x = 0; x < m_width && x < static_cast<int>(values.size()); ++x) {
            double normalizedValue = (values[x] - m_minValue) / (m_maxValue - m_minValue);
            double threshold = 1.0 - (static_cast<double>(y) / m_height);
            
            if (normalizedValue >= threshold) {
                line += m_color + "#" + "\033[0m";
            } else {
                line += " ";
            }
        }
        result[y] = line;
    }
    
    return result;
}

// ============================================================================
// InfoPanel implementation
// ============================================================================
std::vector<std::string> InfoPanel::Render() const {
    std::vector<std::string> result;
    
    // Top border
    std::string topBorder = m_borderColor + "+" + std::string(m_width - 2, '-') + "+" + "\033[0m";
    result.push_back(topBorder);
    
    // Title
    if (!m_title.empty()) {
        std::string titleLine = m_borderColor + "|" + "\033[0m" + 
                               "\033[1m" + " " + m_title + " " + "\033[0m";
        int padding = m_width - 4 - static_cast<int>(m_title.length());
        if (padding > 0) {
            titleLine += std::string(padding, ' ');
        }
        titleLine += m_borderColor + "|" + "\033[0m";
        result.push_back(titleLine);
        
        // Separator
        std::string separator = m_borderColor + "+" + std::string(m_width - 2, '-') + "+" + "\033[0m";
        result.push_back(separator);
    }
    
    // Data rows
    for (const auto& pair : m_data) {
        std::string dataLine = m_borderColor + "|" + "\033[0m" + " ";
        dataLine += pair.first + ": " + "\033[97m" + pair.second + "\033[0m";
        
        int contentWidth = static_cast<int>(pair.first.length() + pair.second.length() + 2);
        int padding = m_width - contentWidth - 4;
        if (padding > 0) {
            dataLine += std::string(padding, ' ');
        }
        dataLine += " " + m_borderColor + "|" + "\033[0m";
        result.push_back(dataLine);
    }
    
    // Bottom border
    std::string bottomBorder = m_borderColor + "+" + std::string(m_width - 2, '-') + "+" + "\033[0m";
    result.push_back(bottomBorder);
    
    return result;
}

// ============================================================================
// ConsoleUI main class implementation
// ============================================================================

// Static variable definitions
bool ConsoleUI::isRunning = false;
std::mutex ConsoleUI::globalOutputMutex;

// Constructor
ConsoleUI::ConsoleUI() 
    : m_hConsole(nullptr)
    , m_hStdIn(nullptr)
    , m_originalConsoleMode(0)
    , m_originalInputMode(0)
    , m_virtualTerminalEnabled(false)
    , m_ansiSupported(false)
    , hSharedMemory(nullptr)
    , pSharedMemory(nullptr)
    , m_headerHeight(5)
    , m_footerHeight(3)
    , m_mainAreaHeight(0)
    , m_contentWidth(0)
    , windowWidth(0)
    , windowHeight(0)
    , m_isRunning(false)
    , m_isInitialized(false)
    , m_needsRefresh(true)
    , m_usingPowerShellUI(false)  // 添加这个缺失的成员变量初始化
    , currentPage(PageType::MAIN_MENU)
    , m_currentSelection(0)
    , selectedMenuItem(0)
    , m_terminalWindowActive(false)
    , m_updateThreadsRunning(false)
    , m_mainWindowActive(false)
    , m_mainWindow(nullptr)
    , m_mainWindowThread(nullptr)
    , m_hListBox(nullptr)
    , m_hInfoArea(nullptr)
    , m_hStatusBar(nullptr)
    , m_dataThreadRunning(false)  // 添加这个缺失的成员变量初始化
{
    // Initialize components
    m_cpuProgressBar = std::make_unique<ProgressBar>(40, 100.0);
    m_memoryProgressBar = std::make_unique<ProgressBar>(40, 100.0);
    m_gpuProgressBar = std::make_unique<ProgressBar>(40, 100.0);
    m_cpuChart = std::make_unique<MiniChart>(50, 6, 60);
    m_tempChart = std::make_unique<MiniChart>(50, 6, 60);
    m_systemPanel = std::make_unique<InfoPanel>("System Info", 60);
    m_hardwarePanel = std::make_unique<InfoPanel>("Hardware Info", 60);
    
    // Set progress bar labels and colors
    m_cpuProgressBar->SetLabel("CPU");
    m_cpuProgressBar->SetColor("\033[94m");  // BRIGHT_BLUE
    
    m_memoryProgressBar->SetLabel("Memory");
    m_memoryProgressBar->SetColor("\033[92m");  // BRIGHT_GREEN
    
    m_gpuProgressBar->SetLabel("GPU");
    m_gpuProgressBar->SetColor("\033[95m");  // BRIGHT_MAGENTA
    
    // Set chart colors
    m_cpuChart->SetColor("\033[94m");  // BRIGHT_BLUE
    m_tempChart->SetColor("\033[91m");  // BRIGHT_RED
    
    m_statusMessage = "PowerShell Terminal UI started";
}

// Destructor
ConsoleUI::~ConsoleUI() {
    if (m_isInitialized) {
        Shutdown();
    }
    
    StopUpdateThreads();
}

// Initialize method
bool ConsoleUI::Initialize() {
    Logger::Info("Initializing PowerShell Terminal UI");
    
    try {
        // Check virtual terminal support
        if (IsVirtualTerminalSupported()) {
            Logger::Info("Virtual terminal support detected, enabling PowerShell UI mode");
            InitializePowerShellTerminal();
        } else {
            Logger::Info("Virtual terminal not supported, using traditional console mode");
            InitializeConsole();
        }
        
        SetupMainMenu();
        CreateTerminalMonitor();
        StartUpdateThreads();
        
        m_isInitialized = true;
        m_isRunning = true;
        
        Logger::Info("PowerShell Terminal UI initialized successfully");
        SetStatus("PowerShell Terminal UI initialized");
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("PowerShell Terminal UI initialization failed: " + std::string(e.what()));
        return false;
    }
}

// Run method
void ConsoleUI::Run() {
    if (!m_isInitialized) {
        Logger::Error("UI not initialized, cannot run");
        return;
    }
    
    Logger::Info("Starting PowerShell Terminal UI main loop");
    
    // Set initial page
    currentPage = PageType::DASHBOARD;
    
    // 根据UI模式选择不同的主循环
    if (m_usingPowerShellUI.load()) {
        while (m_isRunning.load()) {
            try {
                if (m_needsRefresh.load()) {
                    RenderPowerShellLayout();
                    m_needsRefresh = false;
                }
                
                // Handle input
                InputEvent event = WaitForInput();
                HandleInput(event);
                
                // Brief sleep to avoid high CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            catch (const std::exception& e) {
                Logger::Error("PowerShell UI runtime error: " + std::string(e.what()));
                WriteColoredOutput("Runtime error: " + std::string(e.what()) + "\n", "\033[91m");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    } else {
        // 传统控制台主循环
        while (m_isRunning.load()) {
            try {
                if (m_needsRefresh.load()) {
                    RenderMainLayout();  // 修复: 使用现有的RenderMainLayout方法
                    m_needsRefresh = false;
                }
                
                // Handle input
                InputEvent event = WaitForInput();
                HandleInput(event);
                
                // Brief sleep to avoid high CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            catch (const std::exception& e) {
                Logger::Error("Console UI runtime error: " + std::string(e.what()));
                WriteOutput("Runtime error: " + std::string(e.what()) + "\n");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    
    Logger::Info("PowerShell Terminal UI main loop exited");
}

// Shutdown method
void ConsoleUI::Shutdown() {
    Logger::Info("Shutting down PowerShell Terminal UI");
    
    if (m_isInitialized) {
        StopUpdateThreads();
        DestroyTerminalMonitor();
        
        if (m_usingPowerShellUI.load()) {
            RestorePowerShellTerminal();
        } else {
            RestoreConsole();
        }
        
        m_isInitialized = false;
        m_isRunning = false;
    }
}

void ConsoleUI::Stop() {
    m_isRunning = false;
    Logger::Info("PowerShell Terminal UI stopped");
}

void ConsoleUI::Cleanup() {
    if (m_isInitialized) {
        Shutdown();
        Logger::Info("PowerShell Terminal UI cleanup completed");
    }
}

// Basic implementation to avoid linker errors
bool ConsoleUI::IsVirtualTerminalSupported() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }
    
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
}

void ConsoleUI::InitializePowerShellTerminal() {
    // Basic initialization
    m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    m_hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    EnableVirtualTerminal();
    UpdateConsoleSize();
    m_ansiSupported = true;
}

void ConsoleUI::InitializeConsole() {
    m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    m_hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    UpdateConsoleSize();
}

void ConsoleUI::EnableVirtualTerminal() {
    DWORD mode;
    GetConsoleMode(m_hConsole, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    m_virtualTerminalEnabled = SetConsoleMode(m_hConsole, mode) != 0;
}

void ConsoleUI::DisableVirtualTerminal() {
    if (m_virtualTerminalEnabled) {
        DWORD mode;
        GetConsoleMode(m_hConsole, &mode);
        mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(m_hConsole, mode);
        m_virtualTerminalEnabled = false;
    }
}

void ConsoleUI::EnablePowerShellUI(bool enable) {
    m_usingPowerShellUI = enable;
}

bool ConsoleUI::IsPowerShellUIEnabled() const {
    return m_usingPowerShellUI.load();
}

void ConsoleUI::RestorePowerShellTerminal() {
    if (m_hConsole != nullptr) {
        // 显示光标
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(m_hConsole, &cursorInfo);
        cursorInfo.bVisible = TRUE;
        SetConsoleCursorInfo(m_hConsole, &cursorInfo);
        
        // 恢复控制台模式
        if (m_virtualTerminalEnabled) {
            DisableVirtualTerminal();
        }
        
        // 输出退出消息
        WriteColoredOutput("感谢使用 PowerShell 终端UI 系统监控器！\n", "\033[92m");
        WriteOutput("Terminal UI 已安全退出。\n");
    }
}

void ConsoleUI::RestoreConsole() {
    if (m_hConsole != nullptr) {
        // 恢复光标
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(m_hConsole, &cursorInfo);
        cursorInfo.bVisible = TRUE;
        SetConsoleCursorInfo(m_hConsole, &cursorInfo);
        
        // 恢复颜色
        SetConsoleTextAttribute(m_hConsole, m_originalConsoleInfo.wAttributes);
    }
}

void ConsoleUI::UpdateConsoleSize() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(m_hConsole, &csbi)) {
        windowWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        m_contentWidth = windowWidth - 4;
        m_mainAreaHeight = windowHeight - m_headerHeight - m_footerHeight - 2;
    }
}

void ConsoleUI::WriteOutput(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    
    // 使用Windows API而不是std::cout避免缓冲区问题
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
    }
}

void ConsoleUI::WriteColoredOutput(const std::string& text, const std::string& color) {
    if (m_ansiSupported) {
        std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            std::string output = color + text + "\033[0m";
            DWORD written;
            WriteConsoleA(hConsole, output.c_str(), static_cast<DWORD>(output.length()), &written, NULL);
        }
    } else {
        WriteOutput(text);
    }
}

void ConsoleUI::WriteANSI(const std::string& ansiSequence) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    
    // 使用Windows API而不是std::cout避免缓冲区问题
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, ansiSequence.c_str(), static_cast<DWORD>(ansiSequence.length()), &written, NULL);
    }
}

void ConsoleUI::WriteANSIOutput(const std::string& text) {
    if (m_ansiSupported) {
        WriteANSI(text);
    } else {
        WriteOutput(text);
    }
}

void ConsoleUI::ClearScreenPS() {
    WriteANSI(ANSIControl::CLEAR_SCREEN);
    WriteANSI(ANSIControl::CURSOR_HOME);
}

void ConsoleUI::ClearLinePS() {
    WriteANSI(ANSIControl::CLEAR_LINE);
}

void ConsoleUI::MoveCursorPS(int x, int y) {
    std::stringstream ss;
    ss << "\033[" << (y + 1) << ";" << (x + 1) << "H";
    WriteANSI(ss.str());
}

void ConsoleUI::HideCursor() {
    WriteANSI(ANSIControl::CURSOR_HIDE);
}

void ConsoleUI::ShowCursor() {
    WriteANSI(ANSIControl::CURSOR_SHOW);
}

void ConsoleUI::SetupMainMenu() {
    m_mainMenu.clear();
    
    m_mainMenu.push_back({"Dashboard", "Home", "D", [this]() { 
        currentPage = PageType::DASHBOARD; 
        m_needsRefresh = true; 
    }});
    m_mainMenu.push_back({"System Info", "Info", "S", [this]() { 
        currentPage = PageType::SYSTEM_INFO; 
        m_needsRefresh = true; 
    }});
    m_mainMenu.push_back({"Settings", "Config", "T", [this]() { 
        currentPage = PageType::SETTINGS; 
        m_needsRefresh = true; 
    }});
    m_mainMenu.push_back({"About", "Help", "A", [this]() { 
        ShowAbout(); 
    }});
    m_mainMenu.push_back({"Exit", "Quit", "Q", [this]() { 
        m_isRunning = false; 
    }});
}

InputEvent ConsoleUI::WaitForInput() {
    InputEvent event = {};
    if (!m_hStdIn) {
        m_hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    }
    
    if (_kbhit()) {
        int key = _getch();
        event.type = UI_INPUT_KEY;
        event.key.keyCode = key;
    }
    
    return event;
}

void ConsoleUI::HandleInput(const InputEvent& event) {
    if (event.type == UI_INPUT_KEY) {
        switch (event.key.keyCode) {
            case 'q':
            case 'Q':
            case 27: // ESC
                m_isRunning = false;
                break;
            case 13: // Enter
                m_needsRefresh = true;
                break;
            default:
                break;
        }
    }
}

void ConsoleUI::DrawPowerShellHeader() {
    WriteColoredOutput("+", "\033[94m");
    WriteColoredOutput(std::string(windowWidth - 2, '='), "\033[94m");
    WriteColoredOutput("+\n", "\033[94m");
    
    WriteColoredOutput("| ", "\033[94m");
    WriteColoredOutput("Windows System Monitor - PowerShell Terminal UI", "\033[96m");
    int padding = windowWidth - 50;
    WriteOutput(std::string(padding, ' '));
    WriteColoredOutput("|\n", "\033[94m");
    
    WriteColoredOutput("+", "\033[94m");
    WriteColoredOutput(std::string(windowWidth - 2, '='), "\033[94m");
    WriteColoredOutput("+\n", "\033[94m");
}

void ConsoleUI::DrawPowerShellFooter() {
    WriteColoredOutput(std::string(windowWidth, '-'), "\033[90m");
    WriteOutput("\n");
    
    WriteColoredOutput("Controls: ", "\033[96m");
    WriteOutput("Q to quit | Enter to refresh\n");
    
    WriteColoredOutput("Status: ", "\033[92m");
    WriteOutput(GetStatus() + "\n");
}

void ConsoleUI::DrawPowerShellDashboard() {
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        WriteColoredOutput("Warning: Cannot read system data - shared memory unavailable\n", "\033[91m");
        return;
    }
    
    SystemInfo sysInfo = ConvertFromSharedMemory(buffer);
    
    WriteColoredOutput("\nReal-time System Dashboard\n", "\033[93m");
    WriteOutput("============================================================\n");
    
    // CPU info
    WriteColoredOutput("\nCPU Usage\n", "\033[94m");
    m_cpuProgressBar->SetValue(sysInfo.cpuUsage);
    WriteOutput(m_cpuProgressBar->Render() + "\n");
    
    // Memory info
    WriteColoredOutput("\nMemory Usage\n", "\033[92m");
    double memoryUsage = static_cast<double>(sysInfo.usedMemory) / sysInfo.totalMemory * 100.0;
    m_memoryProgressBar->SetValue(memoryUsage);
    WriteOutput(m_memoryProgressBar->Render() + "\n");
    
    // Temperature info
    if (!sysInfo.temperatures.empty()) {
        WriteColoredOutput("\nSystem Temperature\n", "\033[91m");
        for (const auto& temp : sysInfo.temperatures) {
            std::string color = "\033[92m";
            if (temp.second > 80) color = "\033[91m";
            else if (temp.second > 65) color = "\033[93m";
            
            WriteOutput("  " + temp.first + ": ");
            WriteColoredOutput(FormatTemperature(temp.second), color);
            WriteOutput("\n");
        }
    }
}

void ConsoleUI::RenderPowerShellLayout() {
    WriteANSIOutput("\033[2J");  // CLEAR_SCREEN
    WriteANSIOutput("\033[H");   // CURSOR_HOME
    
    DrawPowerShellHeader();
    
    switch (currentPage) {
        case PageType::DASHBOARD:
            DrawPowerShellDashboard();
            break;
        default:
            DrawPowerShellDashboard();
            break;
    }
    
    DrawPowerShellFooter();
}

// Formatting tools
std::string ConsoleUI::FormatBytes(uint64_t bytes) {
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    const double tb = gb * 1024.0;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (bytes >= tb) ss << (bytes / tb) << " TB";
    else if (bytes >= gb) ss << (bytes / gb) << " GB";
    else if (bytes >= mb) ss << (bytes / mb) << " MB";
    else if (bytes >= kb) ss << (bytes / kb) << " KB";
    else ss << bytes << " B";

    return ss.str();
}

std::string ConsoleUI::FormatPercentage(double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << value << "%";
    return ss.str();
}

std::string ConsoleUI::FormatTemperature(double value) {
    std::stringstream ss;
    ss << static_cast<int>(value) << "C";
    return ss.str();
}

std::string ConsoleUI::FormatFrequency(double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (value >= 1000) {
        ss << (value / 1000.0) << " GHz";
    } else {
        ss << value << " MHz";
    }
    return ss.str();
}

void ConsoleUI::SetStatus(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_statusMessage = message;
    m_needsRefresh = true;
}

std::string ConsoleUI::GetStatus() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_statusMessage;
}

void ConsoleUI::CreateTerminalMonitor() {
    m_terminalWindowActive = true;
}

void ConsoleUI::DestroyTerminalMonitor() {
    m_terminalWindowActive = false;
}

void ConsoleUI::StartUpdateThreads() {
    m_updateThreadsRunning = true;
    m_terminalUpdateThread = std::thread(&ConsoleUI::TerminalUpdateLoop, this);
    m_dataUpdateThread = std::thread(&ConsoleUI::DataUpdateLoop, this);
}

void ConsoleUI::StopUpdateThreads() {
    m_updateThreadsRunning = false;
    
    if (m_terminalUpdateThread.joinable()) {
        m_terminalUpdateThread.join();
    }
    
    if (m_dataUpdateThread.joinable()) {
        m_dataUpdateThread.join();
    }
}

void ConsoleUI::TerminalUpdateLoop() {
    while (m_updateThreadsRunning.load() && m_isRunning.load()) {
        try {
            if (currentPage == PageType::DASHBOARD) {
                m_needsRefresh = true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        catch (const std::exception& e) {
            Logger::Error("Terminal update thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void ConsoleUI::DataUpdateLoop() {
    while (m_updateThreadsRunning.load() && m_isRunning.load()) {
        try {
            auto buffer = SharedMemoryManager::GetBuffer();
            if (buffer) {
                SystemInfo sysInfo = ConvertFromSharedMemory(buffer);
                
                if (m_cpuProgressBar) {
                    m_cpuProgressBar->SetValue(sysInfo.cpuUsage);
                }
                
                if (m_memoryProgressBar) {
                    double memoryUsage = static_cast<double>(sysInfo.usedMemory) / sysInfo.totalMemory * 100.0;
                    m_memoryProgressBar->SetValue(memoryUsage);
                }
                
                if (m_cpuChart) {
                    m_cpuChart->AddValue(sysInfo.cpuUsage);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        catch (const std::exception& e) {
            Logger::Error("Data update thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// Stub implementations to avoid linker errors
void ConsoleUI::ShowAbout() { 
    if (m_usingPowerShellUI.load()) {
        WriteColoredOutput("About PowerShell Terminal System Monitor\n", ANSIColors::BRIGHT_CYAN);
        WriteOutput("Version: 1.0\nPress any key to continue...\n");
    } else {
        WriteOutput("About Console System Monitor\nVersion: 1.0\nPress any key to continue...\n");
    }
    _getch();
    currentPage = PageType::DASHBOARD;
    m_needsRefresh = true;
}

// Stub implementations for missing methods
void ConsoleUI::ShowSystemInfo() { 
    if (m_usingPowerShellUI.load()) {
        currentPage = PageType::SYSTEM_INFO;
        m_needsRefresh = true;
    }
}

void ConsoleUI::ShowMainMenu() { 
    if (m_usingPowerShellUI.load()) {
        currentPage = PageType::MAIN_MENU;
        m_needsRefresh = true;
    }
}

void ConsoleUI::ShowRealTimeMonitor() { 
    if (m_usingPowerShellUI.load()) {
        currentPage = PageType::REAL_TIME_MONITOR;
        m_needsRefresh = true;
    }
}

void ConsoleUI::ShowSettings() { 
    if (m_usingPowerShellUI.load()) {
        currentPage = PageType::SETTINGS;
        m_needsRefresh = true;
    }
}

void ConsoleUI::ShowPowerShellDashboard() { 
    if (m_usingPowerShellUI.load()) {
        currentPage = PageType::DASHBOARD;
        m_needsRefresh = true;
    }
}

void ConsoleUI::SafeConsoleOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(globalOutputMutex);
    
    // 使用Windows API而不是std::cout避免缓冲区问题
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
    }
}

// Other minimal stub implementations
std::string ConsoleUI::CenterText(const std::string& text, int width) { return text; }
std::string ConsoleUI::PadLeft(const std::string& text, int width) { return text; }
std::string ConsoleUI::PadRight(const std::string& text, int width) { return text; }
void ConsoleUI::ClearScreen() {
    if (m_usingPowerShellUI.load()) {
        ClearScreenPS();
    } else {
        system("cls");
    }
}
void ConsoleUI::ResizeUI() { }
void ConsoleUI::RefreshDisplay() { }
void ConsoleUI::ClearArea(int x, int y, int width, int height) { }
void ConsoleUI::SetConsoleColor(ConsoleColor color) { }
void ConsoleUI::SetConsoleColor(ConsoleColor textColor, ConsoleColor backgroundColor) { }
void ConsoleUI::ResetConsoleColor() { }
void ConsoleUI::MoveCursor(int x, int y) {
    if (m_usingPowerShellUI.load()) {
        MoveCursorPS(x, y);
    } else {
        COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
        SetConsoleCursorPosition(m_hConsole, coord);
    }
}
void ConsoleUI::DrawBox(int x, int y, int width, int height) { }
void ConsoleUI::DrawHorizontalLine(int x, int y, int length) { }
void ConsoleUI::DrawVerticalLine(int x, int y, int length) { }
int ConsoleUI::GetUserChoice(int minChoice, int maxChoice) { return 1; }
std::string ConsoleUI::GetUserInput(const std::string& prompt) { return ""; }
bool ConsoleUI::GetUserConfirmation(const std::string& prompt) { return true; }
void ConsoleUI::SafeUIOutput(const std::string& text, int x, int y) { 
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    
    // 如果指定了坐标，先移动光标
    if (x >= 0 && y >= 0) {
        MoveCursor(x, y);
    }
    
    // 统一使用Windows API输出
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hConsole, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
    }
}
bool ConsoleUI::IsPowerShellTerminal() { return m_virtualTerminalEnabled; }
std::string ConsoleUI::GetPowerShellVersion() { return "PowerShell 7.x"; }
std::string ConsoleUI::GetTerminalInfo() { return "Windows Terminal"; }
void ConsoleUI::CreateMainWindow() { }
void ConsoleUI::DestroyMainWindow() { }
void ConsoleUI::UpdateMainWindow() { }
void ConsoleUI::UpdateInfoArea() { }
LRESULT CALLBACK ConsoleUI::MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
DWORD WINAPI ConsoleUI::MainWindowThreadProc(LPVOID lpParam) { return 0; }
void ConsoleUI::DisplayCPUInfo() { }
void ConsoleUI::DisplayMemoryInfo() { }
void ConsoleUI::DisplayGPUInfo() { }
void ConsoleUI::DisplayTemperatureInfo() { }
void ConsoleUI::DisplayDiskInfo() { }
void ConsoleUI::DisplayNetworkInfo() { }
void ConsoleUI::DisplaySystemInfo() { }
void ConsoleUI::DisplaySystemInfo(const SystemInfo& sysInfo) { }
void ConsoleUI::DisplayCPUInfo(const SystemInfo& sysInfo) { }
void ConsoleUI::DisplayMemoryInfo(const SystemInfo& sysInfo) { }
void ConsoleUI::DisplayGPUInfo(const SystemInfo& sysInfo) { }
void ConsoleUI::DisplayTemperatureInfo(const SystemInfo& sysInfo) { }
void ConsoleUI::DisplayDiskInfo(const SystemInfo& sysInfo) { }
void ConsoleUI::NavigateToPage(PageType page) { currentPage = page; m_needsRefresh = true; }
void ConsoleUI::HandleMenuNavigation(int direction) { }
void ConsoleUI::DisplayPageContent() { }
void ConsoleUI::HandleKeyInput(int keyCode) { }
void ConsoleUI::HandleMouseInput(int x, int y, bool leftClick, bool rightClick) { }
void ConsoleUI::DrawHeader() { }
void ConsoleUI::DrawFooter() { }
void ConsoleUI::DrawMainMenu() { }
void ConsoleUI::DrawBorder() { }
void ConsoleUI::DrawStatusArea() { }
void ConsoleUI::RenderMainLayout() { }
void ConsoleUI::ShowHeader() { }
void ConsoleUI::ShowFooter() { }
void ConsoleUI::PauseForInput() { }
void ConsoleUI::UpdateTerminalMonitor() { }
void ConsoleUI::DrawPowerShellMainMenu() { }
void ConsoleUI::DrawPowerShellBorder() { }
void ConsoleUI::DrawPowerShellStatusArea() { }
std::string ConsoleUI::CreatePowerShellBox(const std::vector<std::string>& content, const std::string& title, int width) { return ""; }
std::string ConsoleUI::CreatePowerShellSeparator(int width, const std::string& character) { return ""; }
std::string ConsoleUI::GetPowerShellIcon(const std::string& name) { return ""; }
std::string ConsoleUI::GetBoxDrawingChar(const std::string& type) { return ""; }

// PSTerminalUtils namespace implementation
namespace PSTerminalUtils {
    std::string EscapeANSI(const std::string& text) {
        std::string result;
        bool inEscape = false;
        
        for (char c : text) {
            if (c == '\033') {
                inEscape = true;
            } else if (inEscape && c == 'm') {
                inEscape = false;
            } else if (!inEscape) {
                result += c;
            }
        }
        
        return result;
    }
    
    std::string StripANSI(const std::string& text) {
        return EscapeANSI(text);
    }
    
    int GetDisplayWidth(const std::string& text) {
        return static_cast<int>(StripANSI(text).length());
    }
    
    std::vector<std::string> WrapText(const std::string& text, int width) {
        std::vector<std::string> result;
        std::string line;
        
        for (char c : text) {
            line += c;
            if (static_cast<int>(line.length()) >= width || c == '\n') {
                result.push_back(line);
                line.clear();
            }
        }
        
        if (!line.empty()) {
            result.push_back(line);
        }
        
        return result;
    }
    
    std::string PadString(const std::string& text, int width, char padChar) {
        if (static_cast<int>(text.length()) >= width) {
            return text;
        }
        return text + std::string(width - text.length(), padChar);
    }
    
    std::string TruncateString(const std::string& text, int maxWidth, const std::string& suffix) {
        if (static_cast<int>(text.length()) <= maxWidth) {
            return text;
        }
        return text.substr(0, maxWidth - suffix.length()) + suffix;
    }
    
    bool IsPowerShellHost() {
        return GetEnvironmentVariableA("PSModulePath", nullptr, 0) > 0;
    }
    
    std::string GetTerminalType() {
        if (IsPowerShellHost()) {
            return "PowerShell";
        }
        return "Console";
    }
}