#pragma once
#include <windows.h>
#include <string>

// 所有 std::string <-> std::wstring 转换统一使用 UTF-8
// 约定: DataStruct.h 中所有 std::string (例如 DiskData.label / fileSystem, SystemInfo.* 字段)
// 均存放 UTF-8 编码文本，跨进程写入共享内存前再转为 wchar_t。
class WinUtils {
public:
    // 基础 UTF-8 转换实现（抛异常安全，失败返回空）
    static std::string WstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) return {};
        std::string out(size_needed, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), out.data(), size_needed, nullptr, nullptr);
        return out;
    }
    static std::wstring Utf8ToWstring(const std::string& str) {
        if (str.empty()) return {};
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
        if (size_needed <= 0) return {};
        std::wstring out(size_needed, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), out.data(), size_needed);
        return out;
    }

    // 兼容旧命名（保持语义：UTF-8）
    static std::wstring StringToWstring(const std::string& str) { return Utf8ToWstring(str); }
    static std::string WstringToString(const std::wstring& wstr) { return WstringToUtf8(wstr); }

    // 便捷检测：是否可能是 UTF-8（简单快速校验，UI诊断用）
    static bool IsLikelyUtf8(const std::string& s) {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(s.data());
        size_t i = 0, len = s.size();
        while (i < len) {
            unsigned char c = bytes[i];
            if (c < 0x80) { ++i; continue; }
            size_t seqLen = 0;
            if ((c & 0xE0) == 0xC0) seqLen = 2; else if ((c & 0xF0) == 0xE0) seqLen = 3; else if ((c & 0xF8) == 0xF0) seqLen = 4; else return false;
            if (i + seqLen > len) return false;
            for (size_t k = 1; k < seqLen; ++k) if ((bytes[i + k] & 0xC0) != 0x80) return false;
            i += seqLen;
        }
        return true;
    }

    static bool EnablePrivilege(const std::wstring& privilegeName, bool enable = true);
    static bool CheckPrivilege(const std::wstring& privilegeName);
    static bool IsRunAsAdmin();
    static std::string FormatWindowsErrorMessage(DWORD errorCode);
    static std::string GetExecutableDirectory();
};
