# Dependencies management for TCMT Client
# 依赖管理模块

function(tcmt_find_dependencies)
    message(STATUS "Finding dependencies...")

    # 通用依赖检查
    if(NOT CMAKE_CXX_COMPILER_ID)
        message(FATAL_ERROR "C++ compiler not found")
    endif()

    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

    # 平台特定依赖
    if(TCMT_WINDOWS)
        tcmt_find_windows_dependencies()
    elseif(TCMT_MACOS)
        tcmt_find_macos_dependencies()
    endif()

    # 第三方库检查
    tcmt_check_third_party_libs()

    message(STATUS "Dependencies check completed")
endfunction()

# Windows平台依赖
function(tcmt_find_windows_dependencies)
    message(STATUS "  Checking Windows dependencies...")

    # Windows SDK检查
    if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        message(WARNING "Windows SDK version not specified, using default")
        set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.26100.0" CACHE STRING "Windows Target Platform Version" FORCE)
    endif()

    # 检查必要的Windows头文件
    check_include_file_cxx(windows.h HAVE_WINDOWS_H)
    if(NOT HAVE_WINDOWS_H)
        message(FATAL_ERROR "windows.h not found - Windows SDK may not be installed")
    endif()

    check_include_file_cxx(pdh.h HAVE_PDH_H)
    if(NOT HAVE_PDH_H)
        message(FATAL_ERROR "pdh.h not found - Performance Data Helper library missing")
    endif()

    check_include_file_cxx(wbemidl.h HAVE_WBEMIDL_H)
    if(NOT HAVE_WBEMIDL_H)
        message(FATAL_ERROR "wbemidl.h not found - WMI library missing")
    endif()

    # 检查必要的库
    check_library_exists(kernel32 GetSystemTime "" HAVE_KERNEL32)
    check_library_exists(user32 MessageBoxW "" HAVE_USER32)
    check_library_exists(pdh PdhOpenQueryW "" HAVE_PDH_LIB)
    check_library_exists(wbemuuid CLSID_WbemLocator "" HAVE_WBEMUUID_LIB)

    if(NOT HAVE_KERNEL32)
        message(FATAL_ERROR "kernel32 library not found")
    endif()
    if(NOT HAVE_PDH_LIB)
        message(FATAL_ERROR "pdh library not found")
    endif()
    if(NOT HAVE_WBEMUUID_LIB)
        message(FATAL_ERROR "wbemuuid library not found")
    endif()

    # CUDA检查（如果启用）
    if(TCMT_ENABLE_CUDA)
        find_package(CUDAToolkit QUIET)
        if(CUDAToolkit_FOUND)
            message(STATUS "    CUDA found: ${CUDAToolkit_VERSION}")
            check_library_exists(nvml nvmlInit "" HAVE_NVML)
            if(HAVE_NVML)
                message(STATUS "    NVML found")
            else()
                message(WARNING "NVML not found - NVIDIA GPU monitoring may not work")
            endif()
        else()
            message(WARNING "CUDA not found - NVIDIA GPU monitoring disabled")
            set(TCMT_ENABLE_CUDA OFF CACHE BOOL "Enable CUDA support" FORCE)
        endif()
    endif()

    message(STATUS "  Windows dependencies OK")
endfunction()

# macOS平台依赖
function(tcmt_find_macos_dependencies)
    message(STATUS "  Checking macOS dependencies...")

    # 检查macOS框架
    check_include_file_cxx(IOKit/IOKitLib.h HAVE_IOKIT_H)
    check_include_file_cxx(CoreFoundation/CoreFoundation.h HAVE_COREFOUNDATION_H)
    check_include_file_cxx(sys/sysctl.h HAVE_SYSCTL_H)
    check_include_file_cxx(mach/mach.h HAVE_MACH_H)

    if(NOT HAVE_IOKIT_H)
        message(FATAL_ERROR "IOKit framework not found")
    endif()
    if(NOT HAVE_COREFOUNDATION_H)
        message(FATAL_ERROR "CoreFoundation framework not found")
    endif()
    if(NOT HAVE_SYSCTL_H)
        message(FATAL_ERROR "sysctl.h not found")
    endif()
    if(NOT HAVE_MACH_H)
        message(FATAL_ERROR "mach.h not found")
    endif()

    # 检查必要的函数
    check_symbol_exists(sysctlbyname "sys/sysctl.h" HAVE_SYSCTLBYNAME)
    check_symbol_exists(host_statistics "mach/mach_host.h" HAVE_HOST_STATISTICS)
    check_symbol_exists(shm_open "sys/mman.h" HAVE_SHM_OPEN)

    if(NOT HAVE_SYSCTLBYNAME)
        message(FATAL_ERROR "sysctlbyname function not found")
    endif()
    if(NOT HAVE_HOST_STATISTICS)
        message(FATAL_ERROR "host_statistics function not found")
    endif()
    if(NOT HAVE_SHM_OPEN)
        message(FATAL_ERROR "shm_open function not found (POSIX shared memory)")
    endif()

    # 检查Metal框架（用于GPU监控）
    check_include_file_cxx(Metal/Metal.h HAVE_METAL_H)
    if(HAVE_METAL_H)
        message(STATUS "    Metal framework found")
    else()
        message(WARNING "Metal framework not found - GPU monitoring may be limited")
    endif()

    message(STATUS "  macOS dependencies OK")
endfunction()

# 第三方库检查
function(tcmt_check_third_party_libs)
    message(STATUS "  Checking third-party libraries...")

    # 检查第三方库目录是否存在
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party")
        message(STATUS "    Third-party directory exists")

        # 检查LibreHardwareMonitor（仅Windows）
        if(TCMT_WINDOWS AND EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/LibreHardwareMonitor")
            message(STATUS "    LibreHardwareMonitor found")
        endif()

        # 检查其他第三方库
        if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/curl")
            message(STATUS "    curl found")
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/PDCurses")
            message(STATUS "    PDCurses found")
        endif()

    else()
        message(WARNING "Third-party directory not found - some features may be unavailable")
    endif()

    # 检查CPP-parsers子模块
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/CPP-parsers")
        message(STATUS "    CPP-parsers found")
    else()
        message(WARNING "CPP-parsers not found - may need to initialize submodules")
    endif()

    message(STATUS "  Third-party libraries check completed")
endfunction()

# 依赖配置总结
function(tcmt_print_dependencies_summary)
    message(STATUS "========================================")
    message(STATUS "Dependencies Summary")
    message(STATUS "========================================")
    message(STATUS "Platform: ${TCMT_PLATFORM_NAME}")
    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

    if(TCMT_WINDOWS)
        message(STATUS "Windows SDK: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
        if(TCMT_ENABLE_CUDA)
            if(CUDAToolkit_FOUND)
                message(STATUS "CUDA: ${CUDAToolkit_VERSION}")
            else()
                message(STATUS "CUDA: Not found")
            endif()
        endif()
    elseif(TCMT_MACOS)
        message(STATUS "macOS frameworks: IOKit, CoreFoundation")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party")
        message(STATUS "Third-party libraries: Available")
    else()
        message(STATUS "Third-party libraries: Not found")
    endif()

    message(STATUS "========================================")
endfunction()