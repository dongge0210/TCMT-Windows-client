// ConsoleUI.h - 升级为PowerShell终端UI
#pragma once
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <conio.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <functional>
#include <memory>
#include <map>
#include <queue>
#include <atomic>
#include "../core/DataStruct/DataStruct.h"
#include "../core/Utils/Logger.h"  // 使用Logger.h中的ConsoleColor定义

// ANSI颜色代码和终端控制序列 - PowerShell兼容
namespace ANSIColors {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM = "\033[2m";
    constexpr const char* UNDERLINE = "\033[4m";
    constexpr const char* BLINK = "\033[5m";
    constexpr const char* REVERSE = "\033[7m";
    
    // 前景色
    constexpr const char* BLACK = "\033[30m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* WHITE = "\033[37m";
    
    // 明亮前景色
    constexpr const char* BRIGHT_BLACK = "\033[90m";
    constexpr const char* BRIGHT_RED = "\033[91m";
    constexpr const char* BRIGHT_GREEN = "\033[92m";
    constexpr const char* BRIGHT_YELLOW = "\033[93m";
    constexpr const char* BRIGHT_BLUE = "\033[94m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";
    constexpr const char* BRIGHT_CYAN = "\033[96m";
    constexpr const char* BRIGHT_WHITE = "\033[97m";
    
    // 背景色
    constexpr const char* BG_BLACK = "\033[40m";
    constexpr const char* BG_RED = "\033[41m";
    constexpr const char* BG_GREEN = "\033[42m";
    constexpr const char* BG_YELLOW = "\033[43m";
    constexpr const char* BG_BLUE = "\033[44m";
    constexpr const char* BG_MAGENTA = "\033[45m";
    constexpr const char* BG_CYAN = "\033[46m";
    constexpr const char* BG_WHITE = "\033[47m";
}

// 终端控制序列
namespace ANSIControl {
    constexpr const char* CLEAR_SCREEN = "\033[2J";
    constexpr const char* CLEAR_LINE = "\033[2K";
    constexpr const char* CURSOR_HOME = "\033[H";
    constexpr const char* CURSOR_HIDE = "\033[?25l";
    constexpr const char* CURSOR_SHOW = "\033[?25h";
    constexpr const char* SAVE_CURSOR = "\033[s";
    constexpr const char* RESTORE_CURSOR = "\033[u";
}

// 页面类型枚举
enum class PageType {
    MAIN_MENU,
    DASHBOARD,
    CPU_INFO,
    MEMORY_INFO,
    GPU_INFO,
    TEMPERATURE_INFO,
    DISK_INFO,
    NETWORK_INFO,
    SYSTEM_INFO,
    REAL_TIME_MONITOR,  // 添加缺失的枚举值
    SETTINGS
};

// 输入类型枚举
enum InputType {
    UI_INPUT_KEY,
    UI_INPUT_MOUSE,
    UI_INPUT_RESIZE
};

// 输入事件结构
struct InputEvent {
    InputType type;
    union {
        struct {
            int keyCode;
            unsigned long controlKeys;
        } key;
        struct {
            int x, y;
            bool leftClick;
            bool rightClick;
        } mouse;
    };
    
    InputEvent() : type(UI_INPUT_KEY) {
        key.keyCode = 0;
        key.controlKeys = 0;
    }
};

// PowerShell主题配置
struct PowerShellTheme {
    std::string primary;        // 主要颜色
    std::string secondary;      // 次要颜色  
    std::string accent;         // 强调色
    std::string success;        // 成功色
    std::string warning;        // 警告色
    std::string error;          // 错误色
    std::string info;           // 信息色
    std::string text;           // 文本色
    std::string background;     // 背景色
    std::string border;         // 边框色
    std::string highlight;      // 高亮色
    std::string muted;          // 静音色
    
    PowerShellTheme() {
        // 默认深色主题（类似PowerShell 7+）
        primary = ANSIColors::BRIGHT_BLUE;
        secondary = ANSIColors::BRIGHT_CYAN;
        accent = ANSIColors::BRIGHT_MAGENTA;
        success = ANSIColors::BRIGHT_GREEN;
        warning = ANSIColors::BRIGHT_YELLOW;
        error = ANSIColors::BRIGHT_RED;
        info = ANSIColors::BRIGHT_CYAN;
        text = ANSIColors::WHITE;
        background = ANSIColors::BLACK;
        border = ANSIColors::BRIGHT_BLACK;
        highlight = ANSIColors::BG_BLUE;
        muted = ANSIColors::BRIGHT_BLACK;
    }
};

// UI主题配置（保持向后兼容）
struct UITheme {
    ConsoleColor backgroundColor;
    ConsoleColor textColor;
    ConsoleColor headerColor;
    ConsoleColor valueColor;
    ConsoleColor warningColor;
    ConsoleColor successColor;
    ConsoleColor borderColor;
    
    UITheme() : 
        backgroundColor(ConsoleColor::BLACK),
        textColor(ConsoleColor::LIGHT_GRAY),
        headerColor(ConsoleColor::YELLOW),
        valueColor(ConsoleColor::LIGHT_CYAN),
        warningColor(ConsoleColor::LIGHT_RED),
        successColor(ConsoleColor::LIGHT_GREEN),
        borderColor(ConsoleColor::DARK_GRAY) {}
};

// 菜单项结构
struct MenuItem {
    std::string text;
    std::string icon;
    std::string hotkey;
    std::function<void()> action;
    bool enabled;
    bool separator;
    
    MenuItem() : enabled(true), separator(false) {}
    MenuItem(const std::string& txt, std::function<void()> act) : text(txt), action(act), enabled(true), separator(false) {}
    MenuItem(const std::string& txt, const std::string& ico, const std::string& key, std::function<void()> act) 
        : text(txt), icon(ico), hotkey(key), action(act), enabled(true), separator(false) {}
    
    static MenuItem Separator() {
        MenuItem item;
        item.separator = true;
        return item;
    }
};

// 进度条组件
class ProgressBar {
private:
    double m_value;
    double m_maxValue;
    int m_width;
    std::string m_label;
    std::string m_color;
    
public:
    ProgressBar(int width = 40, double maxValue = 100.0)
        : m_value(0), m_maxValue(maxValue), m_width(width), m_color(ANSIColors::BRIGHT_GREEN) {}
    
    void SetValue(double value) { m_value = std::clamp(value, 0.0, m_maxValue); }
    void SetLabel(const std::string& label) { m_label = label; }
    void SetColor(const std::string& color) { m_color = color; }
    std::string Render() const;
    double GetPercentage() const { return (m_value / m_maxValue) * 100.0; }
};

// 迷你图表组件
class MiniChart {
private:
    std::queue<double> m_data;
    int m_maxPoints;
    double m_minValue;
    double m_maxValue;
    int m_width;
    int m_height;
    std::string m_color;
    
public:
    MiniChart(int width = 40, int height = 6, int maxPoints = 60)
        : m_maxPoints(maxPoints), m_minValue(0), m_maxValue(100), 
          m_width(width), m_height(height), m_color(ANSIColors::BRIGHT_CYAN) {}
    
    void AddValue(double value);
    void SetColor(const std::string& color) { m_color = color; }
    std::vector<std::string> Render() const;
    void Clear() { while (!m_data.empty()) m_data.pop(); }
};

// 信息面板组件  
class InfoPanel {
private:
    std::string m_title;
    std::map<std::string, std::string> m_data;
    int m_width;
    std::string m_borderColor;
    
public:
    InfoPanel(const std::string& title, int width = 50)
        : m_title(title), m_width(width), m_borderColor(ANSIColors::BRIGHT_BLACK) {}
    
    void SetData(const std::string& key, const std::string& value) { m_data[key] = value; }
    void SetBorderColor(const std::string& color) { m_borderColor = color; }
    std::vector<std::string> Render() const;
    void Clear() { m_data.clear(); }
};

// 线程安全输出声明
extern std::mutex g_uiConsoleMutex;
void SafeUIOutput(const std::string& message);
void SafeUIOutput(const std::string& message, int color);

// 转换函数声明
SystemInfo ConvertFromSharedMemory(const SharedMemoryBlock* buffer);

// PowerShell终端UI管理器
class ConsoleUI {
private:
    // 控制台相关
    HANDLE m_hConsole;
    HANDLE m_hStdIn;
    CONSOLE_SCREEN_BUFFER_INFO m_originalConsoleInfo;
    DWORD m_originalConsoleMode;
    DWORD m_originalInputMode;
    COORD m_consoleSize;
    
    // 虚拟终端支持
    bool m_virtualTerminalEnabled;
    bool m_ansiSupported;
    
    // 共享内存
    HANDLE hSharedMemory;
    SharedMemoryBlock* pSharedMemory;
    
    // 布局相关
    int m_headerHeight;
    int m_footerHeight;
    int m_mainAreaHeight;
    int m_contentWidth;
    int windowWidth;
    int windowHeight;
    
    // UI状态
    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_isInitialized;
    std::atomic<bool> m_needsRefresh;
    std::atomic<bool> m_usingPowerShellUI;  // 添加这个缺失的成员变量
    PowerShellTheme m_psTheme;
    UITheme m_theme;  // 保持向后兼容
    PageType currentPage;
    
    // 菜单系统
    std::vector<MenuItem> m_mainMenu;
    int m_currentSelection;
    int selectedMenuItem;
    std::vector<std::string> menuItems;
    
    // 状态信息
    std::string m_statusMessage;
    mutable std::mutex m_statusMutex;
    
    // PowerShell终端窗口（替代Win32窗口）
    std::atomic<bool> m_terminalWindowActive;
    std::thread m_terminalUpdateThread;
    std::thread m_dataUpdateThread;
    std::atomic<bool> m_updateThreadsRunning;
    std::atomic<bool> m_dataThreadRunning;  // 添加这个缺失的成员变量
    
    // 实时监控组件
    std::unique_ptr<ProgressBar> m_cpuProgressBar;
    std::unique_ptr<ProgressBar> m_memoryProgressBar;
    std::unique_ptr<ProgressBar> m_gpuProgressBar;
    std::unique_ptr<MiniChart> m_cpuChart;
    std::unique_ptr<MiniChart> m_tempChart;
    std::unique_ptr<InfoPanel> m_systemPanel;
    std::unique_ptr<InfoPanel> m_hardwarePanel;
    
    // 内部方法
    void InitializeConsole();
    void InitializePowerShellTerminal();
    void RestoreConsole();
    void RestorePowerShellTerminal();  // 添加这个缺失的方法声明
    void SetupMainMenu();
    void UpdateConsoleSize();
    void EnableVirtualTerminal();
    void DisableVirtualTerminal();
    bool IsVirtualTerminalSupported();
    void EnablePowerShellUI(bool enable);  // 添加这个缺失的方法声明
    bool IsPowerShellUIEnabled() const;  // 添加这个缺失的方法声明
    
    // 输出方法
    void WriteOutput(const std::string& text);
    void WriteColoredOutput(const std::string& text, const std::string& color);
    void WriteANSI(const std::string& ansiSequence);
    void WriteANSIOutput(const std::string& text);  // 添加这个缺失的方法声明
    void ClearScreenPS();  // 添加这个缺失的方法声明
    void ClearLinePS();  // 添加这个缺失的方法声明
    void MoveCursorPS(int x, int y);  // 添加这个缺失的方法声明
    void HideCursor();  // 添加这个缺失的方法声明
    void ShowCursor();  // 添加这个缺失的方法声明
    
    // 绘制方法（升级为PowerShell风格）
    void DrawPowerShellHeader();
    void DrawPowerShellFooter();
    void DrawPowerShellMainMenu();
    void DrawPowerShellDashboard();
    void DrawPowerShellBorder();
    void DrawPowerShellStatusArea();
    void RenderPowerShellLayout();
    void RenderPowerShellDashboard();  // 添加这个缺失的方法声明
    void RenderPowerShellMainMenu();  // 添加这个缺失的方法声明
    void RenderPowerShellHeader();  // 添加这个缺失的方法声明
    void RenderPowerShellFooter();  // 添加这个缺失的方法声明
    void RenderPowerShellSystemInfo();  // 添加这个缺失的方法声明
    void RenderPowerShellRealTimeMonitor();  // 添加这个缺失的方法声明
    void RenderPowerShellSettings();  // 添加这个缺失的方法声明
    
    // 传统绘制方法（保持兼容）
    void DrawHeader();
    void DrawFooter();
    void DrawMainMenu();
    void DrawBorder();
    void DrawStatusArea();
    void RenderMainLayout();
    void ShowHeader();
    void ShowFooter();
    void PauseForInput();
    
    // 输入处理
    InputEvent GetInput();
    InputEvent WaitForInput();
    void HandleInput(const InputEvent& event);
    void HandlePowerShellInput(const InputEvent& event);  // 添加这个缺失的方法声明
    void HandleKeyInput(int keyCode);
    void HandleMouseInput(int x, int y, bool leftClick, bool rightClick);
    
    // 页面处理
    void NavigateToPage(PageType page);
    void HandleMenuNavigation(int direction);
    void DisplayPageContent();
    
    // PowerShell终端实时监控
    void CreateTerminalMonitor();
    void DestroyTerminalMonitor();
    void UpdateTerminalMonitor();
    void TerminalUpdateLoop();
    void DataUpdateLoop();
    void StartUpdateThreads();
    void StopUpdateThreads();
    void StartDataUpdateThread();  // 添加这个缺失的方法声明
    void StopDataUpdateThread();  // 添加这个缺失的方法声明
    void UpdateSystemData();  // 添加这个缺失的方法声明
    SystemInfo ConvertFromSharedMemory(const SharedMemoryBlock* buffer);  // 添加这个缺失的方法声明
    
    // 工具方法
    std::string CreatePowerShellBox(const std::vector<std::string>& content, const std::string& title = "", int width = 0);
    std::string CreatePowerShellSeparator(int width, const std::string& character = "─");
    std::string GetPowerShellIcon(const std::string& name);
    std::string GetBoxDrawingChar(const std::string& type);
    std::string CreateBox(const std::vector<std::string>& content, const std::string& title = "", int width = 0);  // 添加这个缺失的方法声明
    std::string CreateSeparator(int width, char character = 45);  // 修复: 使用数字45而不是字符'-'
    std::string GetIcon(const std::string& name);  // 添加这个缺失的方法声明
    int GetDisplayWidth(const std::string& text);  // 添加这个缺失的方法声明
    std::string StripANSI(const std::string& text);  // 添加这个缺失的方法声明
    
public:
    // 构造函数和析构函数
    ConsoleUI();
    ~ConsoleUI();
    
    // 主要方法
    bool Initialize();
    void Run();
    void Shutdown();
    void Stop();
    void Cleanup();
    
    // 显示功能
    void ShowMainMenu();
    void ShowSystemInfo();
    void ShowRealTimeMonitor();
    void ShowSettings();
    void ShowAbout();
    void ShowPowerShellDashboard();
    
    // 页面显示方法
    void DisplayCPUInfo();
    void DisplayMemoryInfo();
    void DisplayGPUInfo();
    void DisplayTemperatureInfo();
    void DisplayDiskInfo();
    void DisplayNetworkInfo();
    void DisplaySystemInfo();
    void DisplaySystemInfo(const SystemInfo& sysInfo);
    void DisplayCPUInfo(const SystemInfo& sysInfo);
    void DisplayMemoryInfo(const SystemInfo& sysInfo);
    void DisplayGPUInfo(const SystemInfo& sysInfo);
    void DisplayTemperatureInfo(const SystemInfo& sysInfo);
    void DisplayDiskInfo(const SystemInfo& sysInfo);
    
    // 布局方法
    void ResizeUI();
    void RefreshDisplay();
    
    // 工具方法
    void ClearScreen();
    void ClearArea(int x, int y, int width, int height);
    void SetConsoleColor(ConsoleColor color);
    void SetConsoleColor(ConsoleColor textColor, ConsoleColor backgroundColor);
    void ResetConsoleColor();
    void MoveCursor(int x, int y);
    void DrawBox(int x, int y, int width, int height);
    void DrawHorizontalLine(int x, int y, int length);
    void DrawVerticalLine(int x, int y, int length);
    
    // 状态管理
    void SetStatus(const std::string& message);
    std::string GetStatus() const;
    
    // 主题管理
    void SetPowerShellTheme(const PowerShellTheme& theme) { m_psTheme = theme; }
    PowerShellTheme GetPowerShellTheme() const { return m_psTheme; }
    void SetTheme(const UITheme& theme) { m_theme = theme; }
    const UITheme& GetTheme() const { return m_theme; }
    
    // 输入处理
    int GetUserChoice(int minChoice, int maxChoice);
    std::string GetUserInput(const std::string& prompt);
    bool GetUserConfirmation(const std::string& prompt);
    
    // 线程安全输出
    void SafeUIOutput(const std::string& text, int x = -1, int y = -1);
    
    // 状态管理
    void SetRunning(bool running) { m_isRunning = running; }
    bool IsRunning() const { return m_isRunning.load(); }
    
    // 格式化工具
    std::string FormatBytes(uint64_t bytes);
    std::string FormatPercentage(double value);
    std::string FormatTemperature(double value);
    std::string FormatFrequency(double value);
    std::string CenterText(const std::string& text, int width);
    std::string PadLeft(const std::string& text, int width);
    std::string PadRight(const std::string& text, int width);
    
    // Win32窗口兼容接口（现在映射到PowerShell终端）
    void CreateMainWindow();
    void DestroyMainWindow();
    void UpdateMainWindow();
    void UpdateInfoArea();
    static DWORD WINAPI MainWindowThreadProc(LPVOID lpParam);
    static LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // 新增PowerShell终端特有功能
    bool IsPowerShellTerminal();
    std::string GetPowerShellVersion();
    std::string GetTerminalInfo();
    bool SupportsANSI() const { return m_ansiSupported; }
    bool SupportsVirtualTerminal() const { return m_virtualTerminalEnabled; }
    
    // 静态全局变量和函数
    static bool isRunning;
    static std::mutex globalOutputMutex;
    static void SafeConsoleOutput(const std::string& message);
    
    // 为了保持兼容性，保留原有字段名
    bool m_mainWindowActive;
    HWND m_mainWindow;
    HANDLE m_mainWindowThread;
    HWND m_hListBox;        // 这些在PowerShell模式下不使用
    HWND m_hInfoArea;       // 但保留以兼容现有代码
    HWND m_hStatusBar;      // 
};

// 全局线程安全输出函数
void SafeTerminalOutput(const std::string& message);
void SafeTerminalColorOutput(const std::string& message, const std::string& color);

// 工具函数
namespace PSTerminalUtils {
    std::string EscapeANSI(const std::string& text);
    std::string StripANSI(const std::string& text);
    int GetDisplayWidth(const std::string& text);
    std::vector<std::string> WrapText(const std::string& text, int width);
    std::string PadString(const std::string& text, int width, char padChar = ' ');
    std::string TruncateString(const std::string& text, int maxWidth, const std::string& suffix = "...");
    bool IsPowerShellHost();
    std::string GetTerminalType();
}