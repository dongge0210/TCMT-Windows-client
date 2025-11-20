// Cross-platform stubs for Windows-specific functions
// This file provides minimal implementations for non-Windows platforms

#ifndef CROSS_PLATFORM_STUBS_H
#define CROSS_PLATFORM_STUBS_H

#include <cstring>
#include <string>

#ifdef PLATFORM_WINDOWS
    // Windows implementations - use native APIs
    #include <windows.h>
    #include <wbemidl.h>
    #include <comdef.h>
#else
        // Non-Windows stub implementations
        typedef int BOOL;
        typedef unsigned char BYTE;
        typedef unsigned long DWORD;
        typedef int HANDLE;
        typedef void* HCRYPTPROV;
        typedef void* HCRYPTHASH;
    
    // Basic types and constants
    typedef int BOOL;
    #ifndef TRUE
        #define TRUE 1
    #endif
    #ifndef FALSE
        #define FALSE 0
    #endif
    #ifndef NULL
        #define NULL nullptr
    #endif
    
    // String functions
    #define strcpy_s(dst, size, src) (strncpy(dst, src, size-1), dst[size-1] = '\0')
        #define strncpy_s(dst, size, src, count) do { \
            size_t _n = ((size) > 0) ? (((count) < ((size)-1)) ? (count) : ((size)-1)) : 0; \
            if ((size) > 0) { \
                strncpy((dst), (src), _n); \
                (dst)[_n] = '\0'; \
            } \
        } while(0)
        #define wcscpy_s(dst, size, src) (wcscpy(dst, src))
    
    // Additional wide string functions for non-Windows
    #ifndef wcsncpy_s
        #define wcsncpy_s(dst, size, src, count) (wcsncpy(dst, src, count))
    #endif
    #ifndef wcscat_s
        #define wcscat_s(dst, src) (wcscat(dst, src))
    #endif
    
    // Crypto stubs
    #define PROV_RSA_AES 1
    #define CRYPT_VERIFYCONTEXT 0
    #define CALG_SHA_256 1
    #define HP_HASHVAL 1
    
    inline BOOL CryptAcquireContext(HCRYPTPROV* hProv, const void* container, const void* provider, DWORD prov, DWORD flags) {
        return FALSE;
    }
    
    inline BOOL CryptCreateHash(HCRYPTPROV hProv, DWORD alg, DWORD flags, DWORD type, HCRYPTHASH* hHash) {
        return FALSE;
    }
    
    inline BOOL CryptHashData(HCRYPTHASH hHash, const BYTE* data, DWORD len, DWORD flags) {
        return FALSE;
    }
    
    inline BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD param, BYTE* data, DWORD* len, DWORD flags) {
        return FALSE;
    }
    
    inline BOOL CryptDestroyHash(HCRYPTHASH hHash) {
        return FALSE;
    }
    
    inline BOOL CryptReleaseContext(HCRYPTPROV hProv, DWORD flags) {
        return FALSE;
    }
    
    // WMI stubs
    struct IWbemServices;
    struct IWbemClassObject;
    struct IEnumWbemClassObject;
    
    typedef long HRESULT;
    typedef wchar_t* BSTR;
    typedef struct tagVARIANT VARIANT;
    
    const HRESULT S_OK = 0;
    const HRESULT S_FALSE = 1;
    const DWORD WBEM_INFINITE = 0xFFFFFFFF;
    const DWORD WBEM_FLAG_FORWARD_ONLY = 0x20;
    const DWORD WBEM_FLAG_RETURN_IMMEDIATELY = 0x40;
    const unsigned short VT_BSTR = 8;
    
    template<typename T>
    class bstr_t {
    public:
        bstr_t(const char* s) {}
        bstr_t(const wchar_t* s) {}
    };
    
    inline void VariantInit(VARIANT* var) {}
    
    // Windows API stubs
    inline HANDLE CreateMutexW(void* attr, BOOL initial, const wchar_t* name) {
        return (HANDLE)1; // Stub handle
    }
    
    inline DWORD WaitForSingleObject(HANDLE handle, DWORD timeout) {
        return 0; // WAIT_OBJECT_0
    }
    
    inline BOOL ReleaseMutex(HANDLE handle) {
        return TRUE;
    }
    
    inline DWORD GetLastError() {
        return 0;
    }
    
    const DWORD ERROR_ALREADY_EXISTS = 183;
    
    // Memory functions
    inline void* MapViewOfFile(HANDLE hMap, DWORD access, DWORD offsetHigh, DWORD offsetLow, size_t size) {
        static char stubMemory[3212]; // SharedMemoryBlock size
        return stubMemory;
    }
    
    inline BOOL UnmapViewOfFile(void* addr) {
        return TRUE;
    }
    
    inline HANDLE CreateFileMapping(HANDLE hFile, void* attr, DWORD protect, DWORD maxSizeHigh, DWORD maxSizeLow, const wchar_t* name) {
        return (HANDLE)1; // Stub handle
    }
    
    inline BOOL CloseHandle(HANDLE handle) {
        return TRUE;
    }
    
    // Security stubs
    struct SECURITY_DESCRIPTOR {};
    struct SECURITY_ATTRIBUTES {
        DWORD nLength;
        void* lpSecurityDescriptor;
        BOOL bInheritHandle;
    };
    
    inline BOOL InitializeSecurityDescriptor(SECURITY_DESCRIPTOR* sd, DWORD revision) {
        return TRUE;
    }
    
    inline BOOL SetSecurityDescriptorDacl(SECURITY_DESCRIPTOR* sd, BOOL present, void* dacl, BOOL defaulted) {
        return TRUE;
    }
    
#endif // PLATFORM_WINDOWS

#endif // CROSS_PLATFORM_STUBS_H