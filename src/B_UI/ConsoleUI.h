// ConsoleUI.h - 现代化PowerShell终端UI界面
#pragma once
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <conio.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <sstream>
#include <algorithm>
#include "../core/DataStruct/DataStruct.h"
#include "../core/Utils/Logger.h"  // 使用Logger.h中的ConsoleColor定义

// ANSI颜色代码和终端控制序列
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
    constexpr const char* ENABLE_ALT_SCREEN = "\033[?1049h";
    constexpr const char* DISABLE_ALT_SCREEN = "\033[?1049l";
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
    REAL_TIME_MONITOR,
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
        // 默认深色主题（类似VS Code Dark+）
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

// UI主题配置（保持兼容性）
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
    MenuItem(const std::string& txt, std::function<void()> act) 
        : text(txt), action(act), enabled(true), separator(false) {}
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
    ProgressBar(int width = 50, double maxValue = 100.0);
    void SetValue(double value);
    void SetLabel(const std::string& label);
    void SetColor(const std::string& color);
    std::string Render() const;
    double GetPercentage() const;
};

// 迷你图表组件
class MiniChart {
private:
    std::vector<double> m_data;
    int m_maxPoints;
    double m_minValue;
    double m_maxValue;
    int m_width;
    int m_height;
    std::string m_color;
    
public:
    MiniChart(int width = 40, int height = 8, int maxPoints = 60);
    void AddValue(double value);
    void SetColor(const std::string& color);
    std::vector<std::string> Render() const;
    void Clear();
};

// 信息面板组件
class InfoPanel {
private:
    std::string m_title;
    std::map<std::string, std::string> m_data;
    int m_width;
    std::string m_borderColor;
    
public:
    InfoPanel(const std::string& title, int width = 50);
    void SetData(const std::string& key, const std::string& value);
    void SetBorderColor(const std::string& color);
    std::vector<std::string> Render() const;
    void Clear();
};

// 线程安全输出声明
extern std::mutex g_uiConsoleMutex;
void SafeUIOutput(const std::string& message);
void SafeUIOutput(const std::string& message, int color);

// 控制台UI管理器 - 增强为PowerShell终端UI
class ConsoleUI {
private:
    // 控制台相关
    HANDLE m_hConsole;
    HANDLE m_hStdIn;
    CONSOLE_SCREEN_BUFFER_INFO m_originalConsoleInfo;
    DWORD m_originalConsoleMode;
    DWORD m_originalInputMode;
    COORD m_consoleSize;
    
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
    std::atomic<bool> m_usingPowerShellUI;
    UITheme m_theme;
    PowerShellTheme m_psTheme;
    PageType currentPage;
    
    // 菜单系统
    std::vector<MenuItem> m_mainMenu;
    int m_currentSelection;
    int selectedMenuItem;
    std::vector<std::string> menuItems;
    
    // 状态信息
    std::string m_statusMessage;
    mutable std::mutex m_statusMutex;
    
    // PowerShell终端UI组件
    std::unique_ptr<ProgressBar> m_cpuProgressBar;
    std::unique_ptr<ProgressBar> m_memoryProgressBar;
    std::unique_ptr<ProgressBar> m_gpuProgressBar;
    std::unique_ptr<MiniChart> m_cpuChart;
    std::unique_ptr<MiniChart> m_tempChart;
    std::unique_ptr<InfoPanel> m_systemPanel;
    std::unique_ptr<InfoPanel> m_hardwarePanel;
    
    // 实时数据
    std::thread m_dataUpdateThread;
    std::atomic<bool> m_dataThreadRunning;
    
    // 主UI窗口状态（保持兼容性）
    bool m_mainWindowActive;
    HWND m_mainWindow;
    HANDLE m_mainWindowThread;
    HWND m_hListBox;        // 菜单列表框
    HWND m_hInfoArea;       // 信息显示区域
    HWND m_hStatusBar;      // 状态栏
    
    // 内部方法
    void InitializeConsole();
    void InitializePowerShellTerminal();
    void RestoreConsole();
    void RestorePowerShellTerminal();
    void SetupMainMenu();
    void UpdateConsoleSize();
    void EnableVirtualTerminal();
    void DisableVirtualTerminal();
    
    // PowerShell UI渲染方法
    void RenderPowerShellDashboard();
    void RenderPowerShellMainMenu();
    void RenderPowerShellSystemInfo();
    void RenderPowerShellRealTimeMonitor();
    void RenderPowerShellSettings();
    void RenderPowerShellAbout();
    void RenderPowerShellHeader();
    void RenderPowerShellFooter();
    
    // 传统UI渲染方法（保持兼容）
    void ShowHeader();
    void ShowFooter();
    void PauseForInput();
    void SetupConsoleMode();
    void ResizeConsole();
    void CalculateLayout();
    void UpdateWindowSize();
    
    // 绘制方法
    void DrawHeader();
    void DrawFooter();
    void DrawMainMenu();
    void DrawBorder();
    void DrawStatusArea();
    void RenderMainLayout();
    
    // PowerShell输出方法
    void WriteOutput(const std::string& text);
    void WriteColoredOutput(const std::string& text, const std::string& color);
    void WriteANSI(const std::string& ansiSequence);
    void ClearScreenPS();
    void ClearLinePS();
    void MoveCursorPS(int x, int y);
    void HideCursor();
    void ShowCursor();
    
    // 输入处理
    InputEvent GetInput();
    InputEvent WaitForInput();
    void HandleInput(const InputEvent& event);
    void HandlePowerShellInput(const InputEvent& event);
    void HandleKeyInput(int keyCode);
    void HandleMouseInput(int x, int y, bool leftClick, bool rightClick);
    int WaitForKeyPress();
    void ProcessMenuNavigation(int key);
    
    // 页面处理
    void NavigateToPage(PageType page);
    void HandleMenuNavigation(int direction);
    void DisplayPageContent();
    
    // 数据更新
    void StartDataUpdateThread();
    void StopDataUpdateThread();
    void UpdateSystemData();
    void DataUpdateLoop();
    
    // PowerShell工具方法
    std::string CreateBox(const std::vector<std::string>& content, const std::string& title = "", int width = 0);
    std::string CreateSeparator(int width, char character = '─');
    std::string GetIcon(const std::string& name);
    std::string GetBoxChar(const std::string& type);
    int GetDisplayWidth(const std::string& text);
    std::vector<std::string> WrapText(const std::string& text, int width);
    std::string PadString(const std::string& text, int width, char padChar = ' ');
    std::string TruncateString(const std::string& text, int maxWidth, const std::string& suffix = "...");
    std::string EscapeANSI(const std::string& text);
    std::string StripANSI(const std::string& text);
    
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
    
    // UI模式切换
    void EnablePowerShellUI(bool enable = true);
    bool IsPowerShellUIEnabled() const;
    static bool IsVirtualTerminalSupported();
    static std::string GetPowerShellVersion();
    static std::string GetTerminalInfo();
    
    // 添加缺失的方法声明
    SystemInfo ConvertFromSharedMemory(const SharedMemoryBlock* buffer);
};