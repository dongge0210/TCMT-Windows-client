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

// UI线程安全输出的互斥锁
std::mutex g_uiConsoleMutex;

// ============================================================================
// 进度条组件实现
// ============================================================================

ProgressBar::ProgressBar(int width, double maxValue) 
    : m_value(0.0), m_maxValue(maxValue), m_width(width), m_color(ANSIColors::BRIGHT_GREEN) {}

void ProgressBar::SetValue(double value) {
    m_value = std::clamp(value, 0.0, m_maxValue);
}

void ProgressBar::SetLabel(const std::string& label) {
    m_label = label;
}

void ProgressBar::SetColor(const std::string& color) {
    m_color = color;
}

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
        ss << "█";
    }
    ss << ANSIColors::BRIGHT_BLACK;
    for (int i = filledWidth; i < m_width; ++i) {
        ss << "░";
    }
    ss << ANSIColors::RESET << "] ";
    ss << std::fixed << std::setprecision(1) << percentage << "%";
    
    return ss.str();
}

double ProgressBar::GetPercentage() const {
    return (m_value / m_maxValue) * 100.0;
}

// ============================================================================
// 迷你图表组件实现
// ============================================================================

MiniChart::MiniChart(int width, int height, int maxPoints) 
    : m_maxPoints(maxPoints), m_width(width), m_height(height), 
      m_minValue(0.0), m_maxValue(100.0), m_color(ANSIColors::BRIGHT_CYAN) {}

void MiniChart::AddValue(double value) {
    m_data.push_back(value);
    if (m_data.size() > static_cast<size_t>(m_maxPoints)) {
        m_data.erase(m_data.begin());
    }
    
    // 更新最小最大值
    if (!m_data.empty()) {
        auto minmax = std::minmax_element(m_data.begin(), m_data.end());
        m_minValue = *minmax.first;
        m_maxValue = *minmax.second;
        if (m_maxValue == m_minValue) m_maxValue = m_minValue + 1.0;
    }
}

void MiniChart::SetColor(const std::string& color) {
    m_color = color;
}

std::vector<std::string> MiniChart::Render() const {
    std::vector<std::string> result(m_height);
    
    if (m_data.empty()) {
        for (int i = 0; i < m_height; ++i) {
            result[i] = std::string(m_width, ' ');
        }
        return result;
    }
    
    // 创建图表数据
    for (int y = 0; y < m_height; ++y) {
        std::string line;
        for (int x = 0; x < m_width && x < static_cast<int>(m_data.size()); ++x) {
            double normalizedValue = (m_data[x] - m_minValue) / (m_maxValue - m_minValue);
            double threshold = 1.0 - (static_cast<double>(y) / m_height);
            
            if (normalizedValue >= threshold) {
                line += m_color + "▓" + ANSIColors::RESET;
            } else {
                line += " ";
            }
        }
        result[y] = line;
    }
    
    return result;
}

void MiniChart::Clear() {
    m_data.clear();
}

// ============================================================================
// 信息面板组件实现
// ============================================================================

InfoPanel::InfoPanel(const std::string& title, int width) 
    : m_title(title), m_width(width), m_borderColor(ANSIColors::BRIGHT_BLACK) {}

void InfoPanel::SetData(const std::string& key, const std::string& value) {
    m_data[key] = value;
}

void InfoPanel::SetBorderColor(const std::string& color) {
    m_borderColor = color;
}

std::vector<std::string> InfoPanel::Render() const {
    std::vector<std::string> result;
    
    // 顶部边框
    std::string topBorder = m_borderColor + "┌" + std::string(m_width - 2, '─') + "┐" + ANSIColors::RESET;
    result.push_back(topBorder);
    
    // 标题
    if (!m_title.empty()) {
        std::string titleLine = m_borderColor + "│" + ANSIColors::RESET + 
                               ANSIColors::BOLD + " " + m_title + " " + ANSIColors::RESET;
        int padding = m_width - 4 - static_cast<int>(m_title.length());
        titleLine += std::string(padding, ' ') + m_borderColor + "│" + ANSIColors::RESET;
        result.push_back(titleLine);
        
        // 分隔线
        std::string separator = m_borderColor + "├" + std::string(m_width - 2, '─') + "┤" + ANSIColors::RESET;
        result.push_back(separator);
    }
    
    // 数据行
    for (const auto& pair : m_data) {
        std::string dataLine = m_borderColor + "│" + ANSIColors::RESET + " ";
        dataLine += pair.first + ": " + ANSIColors::BRIGHT_WHITE + pair.second + ANSIColors::RESET;
        
        int contentWidth = static_cast<int>(pair.first.length() + pair.second.length() + 2);
        int padding = m_width - contentWidth - 4;
        if (padding > 0) {
            dataLine += std::string(padding, ' ');
        }
        dataLine += " " + m_borderColor + "│" + ANSIColors::RESET;
        result.push_back(dataLine);
    }
    
    // 底部边框
    std::string bottomBorder = m_borderColor + "└" + std::string(m_width - 2, '─') + "┘" + ANSIColors::RESET;
    result.push_back(bottomBorder);
    
    return result;
}

void InfoPanel::Clear() {
    m_data.clear();
}

// ============================================================================
// 线程安全的UI输出函数
// ============================================================================

void SafeUIOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    std::cout << message << std::flush;
}

void SafeUIOutput(const std::string& message, int color) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalColor = csbi.wAttributes;
    
    SetConsoleTextAttribute(hConsole, color);
    std::cout << message << std::flush;
    SetConsoleTextAttribute(hConsole, originalColor);
}

// ============================================================================
// ConsoleUI 主类实现
// ============================================================================

ConsoleUI::ConsoleUI() 
    : m_hConsole(nullptr)
    , m_hStdIn(nullptr)
    , m_originalConsoleMode(0)
    , m_originalInputMode(0)
    , m_isRunning(false)
    , m_isInitialized(false)
    , m_needsRefresh(true)
    , m_usingPowerShellUI(false)
    , m_currentSelection(0)
    , m_headerHeight(5)
    , m_footerHeight(3)
    , m_mainWindowActive(false)
    , m_mainWindow(nullptr)
    , m_mainWindowThread(nullptr)
    , m_hListBox(nullptr)
    , m_hInfoArea(nullptr)
    , m_hStatusBar(nullptr)
    , m_statusMessage("系统监控已启动")
    , m_dataThreadRunning(false)
{
    // 初始化主题
    m_theme = UITheme();
    m_psTheme = PowerShellTheme();
    
    // 初始化组件
    m_cpuProgressBar = std::make_unique<ProgressBar>(30, 100.0);
    m_memoryProgressBar = std::make_unique<ProgressBar>(30, 100.0);
    m_gpuProgressBar = std::make_unique<ProgressBar>(30, 100.0);
    m_cpuChart = std::make_unique<MiniChart>(50, 8, 50);
    m_tempChart = std::make_unique<MiniChart>(50, 8, 50);
    m_systemPanel = std::make_unique<InfoPanel>("系统信息", 60);
    m_hardwarePanel = std::make_unique<InfoPanel>("硬件信息", 60);
    
    // 设置进度条标签和颜色
    m_cpuProgressBar->SetLabel("CPU");
    m_cpuProgressBar->SetColor(ANSIColors::BRIGHT_BLUE);
    
    m_memoryProgressBar->SetLabel("内存");
    m_memoryProgressBar->SetColor(ANSIColors::BRIGHT_GREEN);
    
    m_gpuProgressBar->SetLabel("GPU");
    m_gpuProgressBar->SetColor(ANSIColors::BRIGHT_MAGENTA);
    
    // 设置图表颜色
    m_cpuChart->SetColor(ANSIColors::BRIGHT_BLUE);
    m_tempChart->SetColor(ANSIColors::BRIGHT_RED);
}

ConsoleUI::~ConsoleUI() {
    if (m_isInitialized) {
        Shutdown();
    }
    
    // 停止数据更新线程
    StopDataUpdateThread();
    
    // 清理主窗口
    DestroyMainWindow();
}

bool ConsoleUI::Initialize() {
    Logger::Info("初始化现代化PowerShell终端UI");
    
    try {
        // 检查虚拟终端支持
        if (IsVirtualTerminalSupported()) {
            Logger::Info("检测到虚拟终端支持，启用PowerShell UI模式");
            EnablePowerShellUI(true);
            InitializePowerShellTerminal();
        } else {
            Logger::Info("虚拟终端不支持，使用传统控制台模式");
            EnablePowerShellUI(false);
            InitializeConsole();
        }
        
        SetupMainMenu();
        
        // 如果不使用PowerShell UI，创建传统窗口
        if (!m_usingPowerShellUI) {
            CreateMainWindow();
        }
        
        m_isInitialized = true;
        m_isRunning = true;
        
        // 启动数据更新线程
        StartDataUpdateThread();
        
        Logger::Info("UI初始化成功");
        
        // 设置初始状态信息
        SetStatus("终端UI初始化成功");
        
        // 连接共享内存
        auto buffer = SharedMemoryManager::GetBuffer();
        if (buffer) {
            SetStatus("共享内存连接已建立，实时数据收集激活");
        } else {
            SetStatus("警告：共享内存不可用");
        }
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::Error("UI initialization failed: " + std::string(e.what()));
        return false;
    }
}

void ConsoleUI::EnablePowerShellUI(bool enable) {
    m_usingPowerShellUI = enable;
}

bool ConsoleUI::IsPowerShellUIEnabled() const {
    return m_usingPowerShellUI;
}

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

std::string ConsoleUI::GetPowerShellVersion() {
    // 简单实现，实际可以调用PowerShell获取版本
    return "PowerShell 7.x";
}

std::string ConsoleUI::GetTerminalInfo() {
    std::stringstream ss;
    ss << "Windows Terminal / Console Host\n";
    ss << "Virtual Terminal: " << (IsVirtualTerminalSupported() ? "Supported" : "Not Supported") << "\n";
    ss << "PowerShell UI: " << (IsPowerShellUIEnabled() ? "Enabled" : "Disabled");
    return ss.str();
}

void ConsoleUI::InitializePowerShellTerminal() {
    // 设置控制台代码页为UTF-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    m_hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    
    if (m_hConsole == INVALID_HANDLE_VALUE || m_hStdIn == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("获取控制台句柄失败");
    }
    
    // 保存原始控制台模式
    GetConsoleMode(m_hConsole, &m_originalConsoleMode);
    GetConsoleMode(m_hStdIn, &m_originalInputMode);
    
    // 启用虚拟终端处理
    EnableVirtualTerminal();
    
    // 设置输入模式
    DWORD inputMode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    inputMode &= ~ENABLE_QUICK_EDIT_MODE;
    SetConsoleMode(m_hStdIn, inputMode);
    
    // 保存原始控制台信息
    if (!GetConsoleScreenBufferInfo(m_hConsole, &m_originalConsoleInfo)) {
        throw std::runtime_error("获取控制台信息失败");
    }
    
    // 更新控制台大小
    UpdateConsoleSize();
    
    // 设置控制台标题
    SetConsoleTitleA("System Monitor - PowerShell Terminal UI");
    
    // 清屏并隐藏光标
    WriteANSI(ANSIControl::CLEAR_SCREEN);
    WriteANSI(ANSIControl::CURSOR_HOME);
    HideCursor();
    
    Logger::Info("PowerShell终端UI初始化完成");
}

void ConsoleUI::InitializeConsole() {
    // 传统控制台初始化（保持兼容性）
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (m_hConsole == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("获取控制台句柄失败");
    }
    
    // 启用虚拟终端处理（如果支持）
    DWORD consoleMode;
    GetConsoleMode(m_hConsole, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(m_hConsole, consoleMode);
    
    // 设置输入模式以支持鼠标输入
    m_hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    if (m_hStdIn != INVALID_HANDLE_VALUE) {
        DWORD inputMode;
        GetConsoleMode(m_hStdIn, &inputMode);
        inputMode |= ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
        inputMode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(m_hStdIn, inputMode);
    }
    
    // 保存原始控制台信息
    if (!GetConsoleScreenBufferInfo(m_hConsole, &m_originalConsoleInfo)) {
        throw std::runtime_error("获取控制台信息失败");
    }
    
    // 设置控制台大小
    UpdateConsoleSize();
    
    // 设置控制台标题
    SetConsoleTitleA("System Monitor - Console UI");
    
    // 隐藏光标
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(m_hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(m_hConsole, &cursorInfo);
}

void ConsoleUI::EnableVirtualTerminal() {
    DWORD mode = m_originalConsoleMode;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(m_hConsole, mode);
}

void ConsoleUI::DisableVirtualTerminal() {
    SetConsoleMode(m_hConsole, m_originalConsoleMode);
}

void ConsoleUI::WriteOutput(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    std::cout << text << std::flush;
}

void ConsoleUI::WriteColoredOutput(const std::string& text, const std::string& color) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    std::cout << color << text << ANSIColors::RESET << std::flush;
}

void ConsoleUI::WriteANSI(const std::string& ansiSequence) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    std::cout << ansiSequence << std::flush;
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

void ConsoleUI::Run() {
    if (!m_isInitialized) {
        Logger::Error("UI not initialized, cannot run");
        return;
    }
    
    Logger::Info("开始运行UI主循环");
    
    // 根据UI模式选择不同的主循环
    if (m_usingPowerShellUI) {
        // PowerShell UI主循环
        currentPage = PageType::DASHBOARD;
        
        while (m_isRunning) {
            try {
                if (m_needsRefresh) {
                    switch (currentPage) {
                        case PageType::DASHBOARD:
                            RenderPowerShellDashboard();
                            break;
                        case PageType::MAIN_MENU:
                            RenderPowerShellMainMenu();
                            break;
                        case PageType::SYSTEM_INFO:
                            RenderPowerShellSystemInfo();
                            break;
                        case PageType::REAL_TIME_MONITOR:
                            RenderPowerShellRealTimeMonitor();
                            break;
                        case PageType::SETTINGS:
                            RenderPowerShellSettings();
                            break;
                        default:
                            RenderPowerShellDashboard();
                            break;
                    }
                    m_needsRefresh = false;
                }
                
                // 处理输入
                InputEvent event = WaitForInput();
                HandlePowerShellInput(event);
                
                // 短暂休眠避免过高CPU占用
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            catch (const std::exception& e) {
                Logger::Error("PowerShell UI runtime error: " + std::string(e.what()));
                WriteColoredOutput("Error occurred: " + std::string(e.what()) + "\n", ANSIColors::BRIGHT_RED);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    } else {
        // 传统UI主循环
        RenderMainLayout();
        
        while (m_isRunning) {
            try {
                if (m_needsRefresh) {
                    RenderMainLayout();
                    m_needsRefresh = false;
                }
                
                InputEvent event = WaitForInput();
                HandleInput(event);
            }
            catch (const std::exception& e) {
                Logger::Error("UI runtime error: " + std::string(e.what()));
                SetConsoleColor(m_theme.warningColor);
                SafeUIOutput("Error occurred: " + std::string(e.what()) + "\n");
                ResetConsoleColor();
                PauseForInput();
            }
        }
        
        // 等待主窗口线程结束
        if (m_mainWindowThread) {
            WaitForSingleObject(m_mainWindowThread, INFINITE);
            CloseHandle(m_mainWindowThread);
            m_mainWindowThread = nullptr;
        }
    }
}

void ConsoleUI::RenderPowerShellDashboard() {
    ClearScreenPS();
    
    // 渲染头部
    RenderPowerShellHeader();
    
    // 获取系统信息
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        MoveCursorPS(2, 5);
        WriteColoredOutput("⚠️  无法读取系统数据: 共享内存不可用", ANSIColors::BRIGHT_RED);
        MoveCursorPS(2, 7);
        WriteColoredOutput("请确保主程序正在运行...", ANSIColors::BRIGHT_YELLOW);
        return;
    }
    
    // 转换系统信息
    SystemInfo sysInfo = ConvertFromSharedMemory(buffer);
    
    int currentY = 4;
    
    // 实时数据仪表板
    MoveCursorPS(2, currentY++);
    WriteColoredOutput("📊 实时系统仪表板", m_psTheme.primary);
    MoveCursorPS(2, currentY++);
    WriteOutput(CreateSeparator(m_consoleSize.X - 4, '═'));
    currentY++;
    
    // CPU信息
    MoveCursorPS(4, currentY++);
    WriteColoredOutput("🔥 CPU 使用率", m_psTheme.accent);
    MoveCursorPS(4, currentY++);
    m_cpuProgressBar->SetValue(sysInfo.cpuUsage);
    WriteOutput(m_cpuProgressBar->Render());
    currentY++;
    
    // 内存信息
    MoveCursorPS(4, currentY++);
    WriteColoredOutput("🧠 内存使用率", m_psTheme.accent);
    MoveCursorPS(4, currentY++);
    double memoryUsage = static_cast<double>(sysInfo.usedMemory) / sysInfo.totalMemory * 100.0;
    m_memoryProgressBar->SetValue(memoryUsage);
    WriteOutput(m_memoryProgressBar->Render());
    currentY++;
    
    // GPU信息（如果有）
    if (!sysInfo.gpuName.empty()) {
        MoveCursorPS(4, currentY++);
        WriteColoredOutput("🎮 GPU 信息", m_psTheme.accent);
        MoveCursorPS(4, currentY++);
        WriteOutput("设备: " + m_psTheme.text + sysInfo.gpuName + ANSIColors::RESET);
        currentY++;
    }
    
    // 温度信息
    if (!sysInfo.temperatures.empty()) {
        MoveCursorPS(4, currentY++);
        WriteColoredOutput("🌡️  系统温度", m_psTheme.accent);
        
        for (const auto& temp : sysInfo.temperatures) {
            MoveCursorPS(6, currentY++);
            std::string color = ANSIColors::BRIGHT_GREEN;
            if (temp.second > 80) color = ANSIColors::BRIGHT_RED;
            else if (temp.second > 65) color = ANSIColors::BRIGHT_YELLOW;
            
            WriteOutput(temp.first + ": ");
            WriteColoredOutput(FormatTemperature(temp.second), color);
        }
        currentY++;
    }
    
    // 渲染底部
    RenderPowerShellFooter();
}

void ConsoleUI::RenderPowerShellMainMenu() {
    ClearScreenPS();
    RenderPowerShellHeader();
    
    int startY = 6;
    int centerX = m_consoleSize.X / 2;
    
    MoveCursorPS(centerX - 10, startY);
    WriteColoredOutput("🏠 主菜单", m_psTheme.primary);
    
    startY += 2;
    
    for (size_t i = 0; i < m_mainMenu.size(); ++i) {
        MoveCursorPS(centerX - 15, startY + static_cast<int>(i));
        
        if (i == static_cast<size_t>(m_currentSelection)) {
            WriteColoredOutput("► ", m_psTheme.accent);
            WriteColoredOutput(m_mainMenu[i].text, m_psTheme.highlight);
        } else {
            WriteOutput("  ");
            WriteColoredOutput(m_mainMenu[i].text, m_psTheme.text);
        }
    }
    
    RenderPowerShellFooter();
}

void ConsoleUI::RenderPowerShellHeader() {
    MoveCursorPS(0, 0);
    
    // 创建头部边框
    std::string headerBorder = m_psTheme.border + "╔" + std::string(m_consoleSize.X - 2, '═') + "╗" + ANSIColors::RESET;
    WriteOutput(headerBorder);
    
    MoveCursorPS(0, 1);
    std::string title = "║ " + m_psTheme.primary + "🖥️  Windows 系统监控器 - PowerShell 终端UI" + ANSIColors::RESET;
    int padding = m_consoleSize.X - 35; // 估算标题长度
    title += std::string(padding, ' ') + m_psTheme.border + "║" + ANSIColors::RESET;
    WriteOutput(title);
    
    MoveCursorPS(0, 2);
    std::string bottomBorder = m_psTheme.border + "╚" + std::string(m_consoleSize.X - 2, '═') + "╝" + ANSIColors::RESET;
    WriteOutput(bottomBorder);
}

void ConsoleUI::RenderPowerShellFooter() {
    int footerY = m_consoleSize.Y - 3;
    
    MoveCursorPS(0, footerY);
    WriteOutput(m_psTheme.border + CreateSeparator(m_consoleSize.X, '─') + ANSIColors::RESET);
    
    MoveCursorPS(2, footerY + 1);
    WriteColoredOutput("🎮 控制: ", m_psTheme.info);
    WriteOutput("↑↓ 导航 | ");
    WriteOutput("Enter 选择 | ");
    WriteOutput("ESC/Q 退出 | ");
    WriteOutput("F5 刷新 | ");
    WriteOutput("M 菜单");
    
    MoveCursorPS(2, footerY + 2);
    WriteColoredOutput("📡 状态: ", m_psTheme.info);
    WriteColoredOutput(GetStatus(), m_psTheme.success);
}

void ConsoleUI::HandlePowerShellInput(const InputEvent& event) {
    if (event.type == UI_INPUT_KEY) {
        switch (event.key.keyCode) {
            case VK_UP:
                if (m_currentSelection > 0) {
                    m_currentSelection--;
                    m_needsRefresh = true;
                }
                break;
                
            case VK_DOWN:
                if (m_currentSelection < static_cast<int>(m_mainMenu.size()) - 1) {
                    m_currentSelection++;
                    m_needsRefresh = true;
                }
                break;
                
            case VK_RETURN:
            case VK_SPACE:
                if (m_currentSelection < static_cast<int>(m_mainMenu.size())) {
                    m_mainMenu[m_currentSelection].action();
                    m_needsRefresh = true;
                }
                break;
                
            case VK_ESCAPE:
            case 'Q':
            case 'q':
                m_isRunning = false;
                break;
                
            case VK_F5:
                m_needsRefresh = true;
                break;
                
            case 'M':
            case 'm':
                currentPage = PageType::MAIN_MENU;
                m_needsRefresh = true;
                break;
                
            case 'D':
            case 'd':
                currentPage = PageType::DASHBOARD;
                m_needsRefresh = true;
                break;
                
            case 'S':
            case 's':
                currentPage = PageType::SYSTEM_INFO;
                m_needsRefresh = true;
                break;
                
            case 'R':
            case 'r':
                currentPage = PageType::REAL_TIME_MONITOR;
                m_needsRefresh = true;
                break;
        }
    } else if (event.type == UI_INPUT_RESIZE) {
        UpdateConsoleSize();
        m_needsRefresh = true;
    }
}

void ConsoleUI::RenderPowerShellSystemInfo() {
    ClearScreenPS();
    RenderPowerShellHeader();
    
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) {
        MoveCursorPS(2, 5);
        WriteColoredOutput("⚠️  无法读取系统信息", ANSIColors::BRIGHT_RED);
        return;
    }
    
    SystemInfo sysInfo = ConvertFromSharedMemory(buffer);
    
    int currentY = 4;
    
    // 系统信息标题
    MoveCursorPS(2, currentY++);
    WriteColoredOutput("📋 详细系统信息", m_psTheme.primary);
    MoveCursorPS(2, currentY++);
    WriteOutput(CreateSeparator(m_consoleSize.X - 4, '═'));
    currentY++;
    
    // CPU详细信息
    MoveCursorPS(2, currentY++);
    WriteColoredOutput("🔥 处理器信息", m_psTheme.accent);
    MoveCursorPS(4, currentY++);
    WriteOutput("名称: " + m_psTheme.text + sysInfo.cpuName + ANSIColors::RESET);
    MoveCursorPS(4, currentY++);
    WriteOutput("物理核心: " + m_psTheme.text + std::to_string(sysInfo.physicalCores) + ANSIColors::RESET);
    MoveCursorPS(4, currentY++);
    WriteOutput("逻辑核心: " + m_psTheme.text + std::to_string(sysInfo.logicalCores) + ANSIColors::RESET);
    MoveCursorPS(4, currentY++);
    WriteOutput("P-Core 频率: " + m_psTheme.text + FormatFrequency(sysInfo.performanceCoreFreq) + ANSIColors::RESET);
    if (sysInfo.efficiencyCoreFreq > 0) {
        MoveCursorPS(4, currentY++);
        WriteOutput("E-Core 频率: " + m_psTheme.text + FormatFrequency(sysInfo.efficiencyCoreFreq) + ANSIColors::RESET);
    }
    MoveCursorPS(4, currentY++);
    WriteOutput("超线程: " + m_psTheme.text + (sysInfo.hyperThreading ? "已启用" : "未启用") + ANSIColors::RESET);
    MoveCursorPS(4, currentY++);
    WriteOutput("虚拟化: " + m_psTheme.text + (sysInfo.virtualization ? "已启用" : "未启用") + ANSIColors::RESET);
    currentY++;
    
    // 内存详细信息
    MoveCursorPS(2, currentY++);
    WriteColoredOutput("🧠 内存信息", m_psTheme.accent);
    MoveCursorPS(4, currentY++);
    WriteOutput("总内存: " + m_psTheme.text + FormatBytes(sysInfo.totalMemory) + ANSIColors::RESET);
    MoveCursorPS(4, currentY++);
    WriteOutput("已用内存: " + m_psTheme.text + FormatBytes(sysInfo.usedMemory) + ANSIColors::RESET);
    MoveCursorPS(4, currentY++);
    WriteOutput("可用内存: " + m_psTheme.text + FormatBytes(sysInfo.availableMemory) + ANSIColors::RESET);
    currentY++;
    
    // GPU详细信息
    if (!sysInfo.gpuName.empty()) {
        MoveCursorPS(2, currentY++);
        WriteColoredOutput("🎮 显卡信息", m_psTheme.accent);
        MoveCursorPS(4, currentY++);
        WriteOutput("名称: " + m_psTheme.text + sysInfo.gpuName + ANSIColors::RESET);
        MoveCursorPS(4, currentY++);
        WriteOutput("品牌: " + m_psTheme.text + sysInfo.gpuBrand + ANSIColors::RESET);
        if (sysInfo.gpuMemory > 0) {
            MoveCursorPS(4, currentY++);
            WriteOutput("显存: " + m_psTheme.text + FormatBytes(sysInfo.gpuMemory) + ANSIColors::RESET);
        }
        currentY++;
    }
    
    RenderPowerShellFooter();
}

void ConsoleUI::RenderPowerShellRealTimeMonitor() {
    ClearScreenPS();
    RenderPowerShellHeader();
    
    int currentY = 4;
    
    MoveCursorPS(2, currentY++);
    WriteColoredOutput("📊 实时性能监控", m_psTheme.primary);
    MoveCursorPS(2, currentY++);
    WriteOutput(CreateSeparator(m_consoleSize.X - 4, '═'));
    currentY++;
    
    // 显示CPU图表
    if (m_cpuChart) {
        MoveCursorPS(2, currentY++);
        WriteColoredOutput("🔥 CPU 使用率历史", m_psTheme.accent);
        
        auto chartLines = m_cpuChart->Render();
        for (const auto& line : chartLines) {
            MoveCursorPS(4, currentY++);
            WriteOutput(line);
        }
        currentY++;
    }
    
    // 显示温度图表
    if (m_tempChart) {
        MoveCursorPS(2, currentY++);
        WriteColoredOutput("🌡️  温度历史", m_psTheme.accent);
        
        auto chartLines = m_tempChart->Render();
        for (const auto& line : chartLines) {
            MoveCursorPS(4, currentY++);
            WriteOutput(line);
        }
    }
    
    RenderPowerShellFooter();
}

void ConsoleUI::RenderPowerShellSettings() {
    ClearScreenPS();
    RenderPowerShellHeader();
    
    int currentY = 6;
    int centerX = m_consoleSize.X / 2;
    
    MoveCursorPS(centerX - 5, currentY++);
    WriteColoredOutput("⚙️  设置", m_psTheme.primary);
    currentY++;
    
    MoveCursorPS(centerX - 15, currentY++);
    WriteOutput("1. 监控频率设置");
    MoveCursorPS(centerX - 15, currentY++);
    WriteOutput("2. UI主题设置");
    MoveCursorPS(centerX - 15, currentY++);
    WriteOutput("3. 显示选项设置");
    MoveCursorPS(centerX - 15, currentY++);
    WriteOutput("4. PowerShell UI 切换");
    MoveCursorPS(centerX - 15, currentY++);
    WriteOutput("5. 返回主菜单");
    
    RenderPowerShellFooter();
}

void ConsoleUI::StartDataUpdateThread() {
    if (m_dataThreadRunning) return;
    
    m_dataThreadRunning = true;
    m_dataUpdateThread = std::thread(&ConsoleUI::DataUpdateLoop, this);
}

void ConsoleUI::StopDataUpdateThread() {
    if (m_dataThreadRunning) {
        m_dataThreadRunning = false;
        if (m_dataUpdateThread.joinable()) {
            m_dataUpdateThread.join();
        }
    }
}

void ConsoleUI::DataUpdateLoop() {
    Logger::Info("数据更新线程已启动");
    
    while (m_dataThreadRunning && m_isRunning) {
        try {
            UpdateSystemData();
            
            // 如果在仪表板或实时监控页面，触发刷新
            if (m_usingPowerShellUI && 
                (currentPage == PageType::DASHBOARD || currentPage == PageType::REAL_TIME_MONITOR)) {
                m_needsRefresh = true;
            }
            
            // 等待2秒
            for (int i = 0; i < 20 && m_dataThreadRunning; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        catch (const std::exception& e) {
            Logger::Error("数据更新线程错误: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    Logger::Info("数据更新线程已退出");
}

void ConsoleUI::UpdateSystemData() {
    auto buffer = SharedMemoryManager::GetBuffer();
    if (!buffer) return;
    
    try {
        SystemInfo sysInfo = ConvertFromSharedMemory(buffer);
        
        // 更新进度条
        if (m_cpuProgressBar) {
            m_cpuProgressBar->SetValue(sysInfo.cpuUsage);
        }
        
        if (m_memoryProgressBar) {
            double memoryUsage = static_cast<double>(sysInfo.usedMemory) / sysInfo.totalMemory * 100.0;
            m_memoryProgressBar->SetValue(memoryUsage);
        }
        
        // 更新图表
        if (m_cpuChart) {
            m_cpuChart->AddValue(sysInfo.cpuUsage);
        }
        
        if (m_tempChart && !sysInfo.temperatures.empty()) {
            // 使用第一个温度传感器的数据
            m_tempChart->AddValue(sysInfo.temperatures[0].second);
        }
        
        // 更新信息面板
        if (m_systemPanel) {
            m_systemPanel->Clear();
            m_systemPanel->SetData("CPU", sysInfo.cpuName);
            m_systemPanel->SetData("使用率", FormatPercentage(sysInfo.cpuUsage));
            m_systemPanel->SetData("内存", FormatBytes(sysInfo.totalMemory));
            m_systemPanel->SetData("GPU", sysInfo.gpuName);
        }
    }
    catch (const std::exception& e) {
        Logger::Error("更新系统数据时发生错误: " + std::string(e.what()));
    }
}

// ============================================================================
// 工具方法实现
// ============================================================================

std::string ConsoleUI::CreateBox(const std::vector<std::string>& content, const std::string& title, int width) {
    if (width == 0) {
        width = 50; // 默认宽度
        for (const auto& line : content) {
            int lineWidth = GetDisplayWidth(line);
            if (lineWidth > width - 4) {
                width = lineWidth + 4;
            }
        }
    }
    
    std::stringstream ss;
    
    // 顶部边框
    ss << m_psTheme.border << "┌" << std::string(width - 2, '─') << "┐" << ANSIColors::RESET << "\n";
    
    // 标题（如果有）
    if (!title.empty()) {
        ss << m_psTheme.border << "│" << ANSIColors::RESET;
        ss << " " << m_psTheme.accent << title << ANSIColors::RESET << " ";
        int padding = width - static_cast<int>(title.length()) - 4;
        ss << std::string(padding, ' ');
        ss << m_psTheme.border << "│" << ANSIColors::RESET << "\n";
        
        // 分隔线
        ss << m_psTheme.border << "├" << std::string(width - 2, '─') << "┤" << ANSIColors::RESET << "\n";
    }
    
    // 内容
    for (const auto& line : content) {
        ss << m_psTheme.border << "│" << ANSIColors::RESET << " ";
        ss << line;
        int padding = width - GetDisplayWidth(line) - 4;
        ss << std::string(padding, ' ');
        ss << " " << m_psTheme.border << "│" << ANSIColors::RESET << "\n";
    }
    
    // 底部边框
    ss << m_psTheme.border << "└" << std::string(width - 2, '─') << "┘" << ANSIColors::RESET;
    
    return ss.str();
}

std::string ConsoleUI::CreateSeparator(int width, char character) {
    return std::string(width, character);
}

std::string ConsoleUI::GetIcon(const std::string& name) {
    static std::map<std::string, std::string> icons = {
        {"cpu", "🔥"},
        {"memory", "🧠"},
        {"gpu", "🎮"},
        {"disk", "💾"},
        {"network", "🌐"},
        {"temperature", "🌡️"},
        {"dashboard", "📊"},
        {"settings", "⚙️"},
        {"info", "ℹ️"},
        {"warning", "⚠️"},
        {"error", "❌"},
        {"success", "✅"}
    };
    
    auto it = icons.find(name);
    return (it != icons.end()) ? it->second : "🔸";
}

int ConsoleUI::GetDisplayWidth(const std::string& text) {
    // 简化实现：假设每个字符宽度为1
    // 实际应该考虑ANSI序列和Unicode字符宽度
    return static_cast<int>(StripANSI(text).length());
}

std::string ConsoleUI::StripANSI(const std::string& text) {
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

// ============================================================================
// 传统UI方法实现（保持兼容性）
// ============================================================================

void ConsoleUI::SetupMainMenu() {
    m_mainMenu.clear();
    
    if (m_usingPowerShellUI.load()) {  // 修复: 使用.load()获取原子变量的值
        // PowerShell UI菜单
        m_mainMenu.push_back({"📊 仪表板", "🏠", "D", [this]() { 
            currentPage = PageType::DASHBOARD; 
            m_needsRefresh = true; 
        }});
        m_mainMenu.push_back({"📋 系统概览", "ℹ️", "S", [this]() { 
            currentPage = PageType::SYSTEM_INFO; 
            m_needsRefresh = true; 
        }});
        m_mainMenu.push_back({"📊 实时监控", "📈", "R", [this]() { 
            currentPage = PageType::REAL_TIME_MONITOR; 
            m_needsRefresh = true; 
        }});
        m_mainMenu.push_back({"⚙️ 设置", "🔧", "T", [this]() { 
            currentPage = PageType::SETTINGS; 
            m_needsRefresh = true; 
        }});
        m_mainMenu.push_back({"ℹ️ 关于", "📖", "A", [this]() { 
            ShowAbout(); 
        }});
        m_mainMenu.push_back({"🚪 退出", "❌", "Q", [this]() { 
            m_isRunning = false; 
        }});
    } else {
        // 传统UI菜单
        m_mainMenu.push_back({"系统概览", [this]() { ShowSystemInfo(); }});
        m_mainMenu.push_back({"实时监控", [this]() { ShowRealTimeMonitor(); }});
        m_mainMenu.push_back({"设置", [this]() { ShowSettings(); }});
        m_mainMenu.push_back({"关于", [this]() { ShowAbout(); }});
        m_mainMenu.push_back({"退出", [this]() { m_isRunning = false; }});
    }
}

SystemInfo ConsoleUI::ConvertFromSharedMemory(const SharedMemoryBlock* buffer) {
    SystemInfo sysInfo;
    
    if (!buffer) return sysInfo;
    
    // 基本信息
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
    return sysInfo;
}

void ConsoleUI::Shutdown() {
    Logger::Info("关闭终端UI");
    
    if (m_isInitialized) {
        // 停止数据更新线程
        StopDataUpdateThread();
        
        if (m_usingPowerShellUI) {
            RestorePowerShellTerminal();
        } else {
            RestoreConsole();
        }
        
        m_isInitialized = false;
        m_isRunning = false;
    }
}

void ConsoleUI::RestorePowerShellTerminal() {
    if (m_hConsole != nullptr) {
        // 显示光标
        ShowCursor();
        
        // 清屏
        ClearScreenPS();
        
        // 恢复控制台模式
        SetConsoleMode(m_hConsole, m_originalConsoleMode);
        SetConsoleMode(m_hStdIn, m_originalInputMode);
        
        // 输出退出消息
        WriteColoredOutput("感谢使用 PowerShell 终端UI 系统监控器！\n", ANSIColors::BRIGHT_GREEN);
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

// ============================================================================
// 传统UI方法的其余实现（为了完整性）
// ============================================================================

void ConsoleUI::ClearScreen() {
    if (m_usingPowerShellUI) {
        ClearScreenPS();
    } else {
        system("cls");
    }
}

void ConsoleUI::ClearArea(int x, int y, int width, int height) {
    for (int i = 0; i < height; ++i) {
        MoveCursor(x, y + i);
        SafeUIOutput(std::string(width, ' '));
    }
}

void ConsoleUI::SetConsoleColor(ConsoleColor color) {
    SetConsoleTextAttribute(m_hConsole, static_cast<WORD>(color));
}

void ConsoleUI::SetConsoleColor(ConsoleColor textColor, ConsoleColor backgroundColor) {
    WORD attributes = static_cast<WORD>(textColor) | (static_cast<WORD>(backgroundColor) << 4);
    SetConsoleTextAttribute(m_hConsole, attributes);
}

void ConsoleUI::ResetConsoleColor() {
    SetConsoleColor(m_theme.textColor, m_theme.backgroundColor);
}

void ConsoleUI::MoveCursor(int x, int y) {
    if (m_usingPowerShellUI) {
        MoveCursorPS(x, y);
    } else {
        COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
        SetConsoleCursorPosition(m_hConsole, coord);
    }
}

void ConsoleUI::DrawBox(int x, int y, int width, int height) {
    SetConsoleColor(m_theme.borderColor);
    
    // 使用ASCII字符绘制边框
    MoveCursor(x, y);
    SafeUIOutput("+" + std::string(width - 2, '-') + "+");
    
    for (int i = 1; i < height - 1; ++i) {
        MoveCursor(x, y + i);
        SafeUIOutput("|" + std::string(width - 2, ' ') + "|");
    }
    
    MoveCursor(x, y + height - 1);
    SafeUIOutput("+" + std::string(width - 2, '-') + "+");
}

void ConsoleUI::DrawHorizontalLine(int x, int y, int length) {
    SetConsoleColor(m_theme.borderColor);
    MoveCursor(x, y);
    SafeUIOutput(std::string(length, '-'));
}

void ConsoleUI::DrawVerticalLine(int x, int y, int length) {
    SetConsoleColor(m_theme.borderColor);
    for (int i = 0; i < length; ++i) {
        MoveCursor(x, y + i);
        SafeUIOutput("|");
    }
}

void ConsoleUI::ShowHeader() {
    SetConsoleColor(m_theme.headerColor);
    SafeUIOutput("+===============================================================================+\n");
    SafeUIOutput("|" + CenterText("Windows 系统监控器 - 控制台界面", 79) + "|\n");
    SafeUIOutput("+===============================================================================+\n");
}

void ConsoleUI::ShowFooter() {
    SetConsoleColor(m_theme.borderColor);
    SafeUIOutput("\n");
    SafeUIOutput("使用方向键选择菜单项，按Enter确认，ESC退出\n");
    SafeUIOutput("-------------------------------------------------------------------------------\n");
}

void ConsoleUI::PauseForInput() {
    SafeUIOutput("\n");
    SetConsoleColor(m_theme.headerColor);
    SafeUIOutput("按任意键继续...");
    ResetConsoleColor();
    _getch();
}

int ConsoleUI::GetUserChoice(int minChoice, int maxChoice) {
    std::string input;
    std::getline(std::cin, input);
    
    try {
        int choice = std::stoi(input);
        if (choice >= minChoice && choice <= maxChoice) {
            return choice;
        }
    }
    catch (...) {
        // 输入不是数字
    }
    
    return -1; // 无效选择
}

std::string ConsoleUI::GetUserInput(const std::string& prompt) {
    SetConsoleColor(m_theme.headerColor);
    SafeUIOutput(prompt);
    ResetConsoleColor();
    
    std::string input;
    std::getline(std::cin, input);
    return input;
}

bool ConsoleUI::GetUserConfirmation(const std::string& prompt) {
    SetConsoleColor(m_theme.headerColor);
    SafeUIOutput(prompt + " (y/n): ");
    ResetConsoleColor();
    
    char ch = _getch();
    SafeUIOutput(std::string(1, ch) + "\n");
    return (ch == 'y' || ch == 'Y');
}

void ConsoleUI::SafeUIOutput(const std::string& text, int x, int y) {
    std::lock_guard<std::mutex> lock(g_uiConsoleMutex);
    
    // 如果指定了坐标，先移动光标
    if (x >= 0 && y >= 0) {
        MoveCursor(x, y);
    }
    
    if (m_usingPowerShellUI) {
        std::cout << text << std::flush;
    } else {
        // 使用 WriteConsoleA 而不是 std::cout 避免流缓冲区问题
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD written;
        WriteConsoleA(hConsole, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
    }
}

// ============================================================================
// 兼容性保留方法（避免链接错误）
// ============================================================================

void ConsoleUI::RenderMainLayout() {
    if (m_usingPowerShellUI) {
        RenderPowerShellDashboard();
    } else {
        // 传统主布局渲染
        ClearScreen();
        UpdateConsoleSize();
        
        m_mainAreaHeight = m_consoleSize.Y - m_headerHeight - m_footerHeight - 2;
        m_contentWidth = m_consoleSize.X - 4;
        
        ShowHeader();
        
        MoveCursor(0, m_headerHeight);
        DrawBox(1, m_headerHeight, m_contentWidth + 2, m_mainAreaHeight);
        
        MoveCursor(2, m_headerHeight + 2);
        SafeUIOutput(CenterText("控制台菜单", m_contentWidth) + "\n");
        SafeUIOutput("\n");
        
        for (size_t i = 0; i < m_mainMenu.size(); ++i) {
            MoveCursor(4, m_headerHeight + 4 + static_cast<int>(i));
            if (i == static_cast<size_t>(m_currentSelection)) {
                SetConsoleColor(m_theme.headerColor);
                SafeUIOutput("  > " + std::to_string(i + 1) + ". " + m_mainMenu[i].text);
            } else {
                SetConsoleColor(m_theme.textColor);
                SafeUIOutput("    " + std::to_string(i + 1) + ". " + m_mainMenu[i].text);
            }
            ResetConsoleColor();
        }
        
        ShowFooter();
    }
}

void ConsoleUI::DrawStatusArea() {
    int statusAreaTop = m_headerHeight + m_mainAreaHeight - 4;
    
    MoveCursor(4, statusAreaTop);
    SetConsoleColor(m_theme.headerColor);
    SafeUIOutput("系统状态:");
    ResetConsoleColor();
    
    MoveCursor(4, statusAreaTop + 1);
    SetConsoleColor(m_theme.valueColor);
    std::lock_guard<std::mutex> lock(m_statusMutex);
    SafeUIOutput(m_statusMessage);
    ResetConsoleColor();
}

// Windows原生UI兼容性保留方法
void ConsoleUI::CreateMainWindow() { /* PowerShell UI模式下不需要 */ }
void ConsoleUI::DestroyMainWindow() { /* PowerShell UI模式下不需要 */ }
void ConsoleUI::UpdateMainWindow() { /* PowerShell UI模式下不需要 */ }
void ConsoleUI::UpdateInfoArea() { /* PowerShell UI模式下不需要 */ }

LRESULT CALLBACK ConsoleUI::MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI ConsoleUI::MainWindowThreadProc(LPVOID lpParam) {
    return 0;
}

// 静态变量定义
bool ConsoleUI::isRunning = false;
std::mutex ConsoleUI::globalOutputMutex;

void ConsoleUI::SafeConsoleOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(globalOutputMutex);
    std::cout << message << std::flush;
}
