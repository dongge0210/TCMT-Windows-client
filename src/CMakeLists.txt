# TCMT Client core library build
# Cross-platform build with conditional compilation

message(STATUS "Configuring TCMTClient build system...")

# ============================================================
# Source files (cross-platform)
# ============================================================
set(PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Platform/Platform.cpp
)

set(UTILS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/WinUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/ComInitializationHelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/TimeUtils.cpp
)

# Core modules
set(CORE_MODULES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/cpu/CpuInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/gpu/GpuInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/memory/MemoryInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/disk/DiskInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/network/NetworkAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/os/OSInfo.cpp
)

# DataStruct / SharedMemory
set(DATASTRUCT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/DataStruct/DataStruct.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/DataStruct/Producer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/DataStruct/SharedMemoryManager.cpp
)

# Temperature modules (Windows only for now)
set(TEMP_SOURCES "")
set(TEMP_SOURCES_WINDOWS "")
if(TCMT_WINDOWS)
    list(APPEND TEMP_SOURCES_WINDOWS
        ${CMAKE_CURRENT_SOURCE_DIR}/core/temperature/LibreHardwareMonitorBridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/temperature/TemperatureWrapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/LibreHardwareMonitorBridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/WMIManager.cpp
    )
    list(APPEND TEMP_SOURCES ${TEMP_SOURCES_WINDOWS})
endif()

if(TCMT_MACOS)
    list(APPEND TEMP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/core/temperature/TemperatureWrapper.cpp
    )
endif()

# TUI module (macOS only)
set(TUI_SOURCES "")
if(TCMT_MACOS)
    list(APPEND TUI_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tui/TuiApp.cpp
    )
    set(CURSES_NEED_WIDE TRUE)
    # Prefer Homebrew ncurses (has wide chars) over system SDK (no ncursesw)
    if(APPLE)
        find_program(BREW brew)
        if(BREW)
            execute_process(COMMAND ${BREW} --prefix ncurses
                OUTPUT_VARIABLE NCURSES_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE _brew_rc)
            if(_brew_rc EQUAL 0)
                set(CURSES_INCLUDE_PATH "${NCURSES_PREFIX}/include" CACHE PATH "Homebrew ncurses include")
                list(APPEND CMAKE_PREFIX_PATH "${NCURSES_PREFIX}")
            endif()
        endif()
    endif()
    find_package(Curses REQUIRED)
endif()

# ============================================================
# Build TCMTCore static library
# ============================================================
add_library(TCMTCore STATIC
    ${PLATFORM_SOURCES}
    ${UTILS_SOURCES}
    ${CORE_MODULES_SOURCES}
    ${DATASTRUCT_SOURCES}
    ${TEMP_SOURCES}
    ${TUI_SOURCES}
)

set_target_properties(TCMTCore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(TCMTCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# Platform-specific defines
if(TCMT_WINDOWS)
    target_compile_definitions(TCMTCore PRIVATE
        TCMT_WINDOWS UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )

elseif(TCMT_MACOS)
    target_compile_definitions(TCMTCore PRIVATE
        TCMT_MACOS _DARWIN_C_SOURCE
    )
endif()

# Apply compile options from cmake/Options.cmake
tcmt_target_set_options(TCMTCore)
tcmt_set_platform_libraries(TCMTCore)
tcmt_set_output_directories(TCMTCore)

# Link ncurses on macOS
if(TCMT_MACOS)
    target_link_libraries(TCMTCore PRIVATE ${CURSES_LIBRARIES})
    target_include_directories(TCMTCore PRIVATE ${CURSES_INCLUDE_DIR})
endif()

# ============================================================
# Build executable
# ============================================================
if(TCMT_WINDOWS)
    # Windows: use main.cpp
    add_executable(TCMTClient
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        # Windows-specific GPU bridge (C++/CLI)
        ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/LibreHardwareMonitorBridge.cpp
    )

    # C++/CLI support for Windows
    if(MSVC)
        set_source_files_properties(
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/LibreHardwareMonitorBridge.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/temperature/LibreHardwareMonitorBridge.cpp
            PROPERTIES
            COMPILE_FLAGS "/clr"
        )
    endif()

elseif(TCMT_MACOS)
    # macOS: use main_mac.cpp
    add_executable(TCMTClient
        ${CMAKE_CURRENT_SOURCE_DIR}/main_mac.cpp
    )
endif()

target_link_libraries(TCMTClient PRIVATE TCMTCore)

set_target_properties(TCMTClient PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Platform-specific exe settings
if(TCMT_WINDOWS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(TCMTClient PRIVATE
            /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup
        )
    endif()
elseif(TCMT_MACOS)
    # macOS executable settings
endif()

message(STATUS "TCMTClient build configured successfully")
