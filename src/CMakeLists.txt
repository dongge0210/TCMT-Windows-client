# TCMT Client core library build
# Cross-platform build with conditional compilation

message(STATUS "Configuring TCMT-M build system...")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/core/board/BoardInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/power/PowerInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/usb/UsbInfo.cpp
)

# History / SQLite database logger (cross-platform)
set(HISTORY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/history/HistoryLogger.cpp
)

# DataStruct / SharedMemory
set(DATASTRUCT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/DataStruct/DataStruct.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/DataStruct/Producer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/IPC/SharedMemoryManager.cpp
)

# Application config (CPP-parsers + nlohmann/json wrapper)
set(CONFIG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Config/ConfigManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Config/ConfigManager.cpp
)

# IPC module
set(IPC_SOURCES "")

# IPC command handler (cross-platform)
list(APPEND IPC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/IPC/IpcCommandHandler.cpp
)

if(TCMT_MACOS)
    list(APPEND IPC_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/core/IPC/IPCServer.cpp
    )
endif()

# Temperature modules (Windows only for now)
set(TEMP_SOURCES "")
set(TEMP_SOURCES_WINDOWS "")
if(TCMT_WINDOWS)
    list(APPEND TEMP_SOURCES_WINDOWS
        ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils/WMIManager.cpp
    )
    list(APPEND TEMP_SOURCES ${TEMP_SOURCES_WINDOWS})
    # Note: TemperatureWrapper.cpp and LibreHardwareMonitorBridge.cpp are C++/CLI
    # and are compiled via MSBuild (TCMT.vcxproj), not CMake
endif()

if(TCMT_MACOS)
    list(APPEND TEMP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/core/temperature/TemperatureWrapper.cpp
    )
endif()

# TUI module (macOS only with CMake; Windows via MSBuild)
set(TUI_SOURCES "")
if(TCMT_MACOS)
    list(APPEND TUI_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tui/TuiApp.cpp
    )
endif()

# Curses library - platform specific
if(TCMT_MACOS)
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
    ${CONFIG_SOURCES}
    ${HISTORY_SOURCES}
    ${IPC_SOURCES}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Utils

    # CPP-parsers unified config interface
    ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/src

    # nlohmann/json (header-only) — single include
    ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/json/single_include

    # websocketpp (header-only, for future use)
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/websocketpp

    # History / SQLite database logger
    ${CMAKE_CURRENT_SOURCE_DIR}/core/history

    # New hardware modules
    ${CMAKE_CURRENT_SOURCE_DIR}/core/board
    ${CMAKE_CURRENT_SOURCE_DIR}/core/power
    ${CMAKE_CURRENT_SOURCE_DIR}/core/usb
)

# ASIO (standalone, header-only) — brew on macOS, submodule on Windows
set(ASIO_INCLUDE_DIR "" CACHE PATH "Path to ASIO include directory (asio/version.hpp)")
if(NOT ASIO_INCLUDE_DIR)
    find_path(ASIO_INCLUDE_DIR asio/version.hpp
        HINTS
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include
    )
endif()
if(ASIO_INCLUDE_DIR)
    target_include_directories(TCMTCore PRIVATE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(TCMTCore PRIVATE ASIO_STANDALONE)
    message(STATUS "ASIO found: ${ASIO_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "ASIO not found. Install via 'brew install asio' or add as submodule at third_party/asio")
endif()

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

# CUDA include path (Windows, for nvml.h)
if(TCMT_WINDOWS AND DEFINED ENV{CUDA_PATH})
    target_include_directories(TCMTCore PUBLIC $ENV{CUDA_PATH}/include)
endif()

# Apply compile options from cmake/Options.cmake
tcmt_target_set_options(TCMTCore)
tcmt_set_platform_libraries(TCMTCore)
tcmt_set_output_directories(TCMTCore)

# ============================================================
# CPP-parsers — compile config parsers (macOS)
# ============================================================
if(TCMT_MACOS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/json build-cpp-extern-json)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/yaml-cpp build-cpp-extern-yaml)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/tinyxml2 build-cpp-extern-xml)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/tomlplusplus build-cpp-extern-toml)

    set(CPP_PARSERS_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/src/IniConfigParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/src/TomlConfigParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/src/XmlConfigParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/src/YamlConfigParser.cpp
    )
    add_library(CPPParsers STATIC ${CPP_PARSERS_SRCS})

    target_include_directories(CPPParsers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/src
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/json/single_include
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/yaml-cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/tinyxml2
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/tomlplusplus/include
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/inih
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/inih/cpp
    )

    # inih has no CMake — add its source manually
    target_sources(CPPParsers PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/inih/ini.c
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP-parsers/extern/inih/cpp/INIReader.cpp
    )

    target_link_libraries(CPPParsers PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp
        tinyxml2
        tomlplusplus_tomlplusplus
    )

    target_link_libraries(TCMTCore PRIVATE CPPParsers)
endif()

# Link curses on Windows and macOS
if(TCMT_WINDOWS)
    # Windows: PDCurses - link with prebuilt or skip for now
    # target_link_libraries(TCMTCore PRIVATE ${CURSES_LIBRARY})
    # target_include_directories(TCMTCore PRIVATE ${CURSES_INCLUDE_PATH})
    # For now, skip - will be added after PDCurses is built

elseif(TCMT_MACOS)
    target_link_libraries(TCMTCore PRIVATE ${CURSES_LIBRARIES})
    target_include_directories(TCMTCore PRIVATE ${CURSES_INCLUDE_DIR})
endif()

# SQLite3 - HistoryLogger (cross-platform, via vcpkg or system)
# Install: vcpkg install sqlite3
find_package(unofficial-sqlite3 CONFIG QUIET)
if(NOT unofficial-sqlite3_FOUND)
    find_package(SQLite3 QUIET)
endif()
if(unofficial-sqlite3_FOUND OR SQLite3_FOUND)
    if(unofficial-sqlite3_FOUND)
        target_link_libraries(TCMTCore PRIVATE unofficial::sqlite3::sqlite3)
        target_compile_definitions(TCMTCore PRIVATE HAS_SQLITE3)
    else()
        target_link_libraries(TCMTCore PRIVATE SQLite::SQLite3)
        target_compile_definitions(TCMTCore PRIVATE HAS_SQLITE3)
    endif()
    message(STATUS "SQLite3 found - HistoryLogger enabled")
else()
    message(WARNING "SQLite3 not found (vcpkg: sqlite3). HistoryLogger will be disabled.")
    message(WARNING "Install via: vcpkg install sqlite3")
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/core/history/HistoryLogger.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
endif()

# ============================================================
# Build executable
# ============================================================
if(TCMT_WINDOWS)
    # Windows: use main.cpp (with TUI support via #ifdef)
    add_executable(TCMT-M
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
    add_executable(TCMT-M
        ${CMAKE_CURRENT_SOURCE_DIR}/main_mac.cpp
    )
endif()

target_link_libraries(TCMT-M PRIVATE TCMTCore)

set_target_properties(TCMT-M PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Platform-specific exe settings
if(TCMT_WINDOWS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(TCMT-M PRIVATE
            /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup
        )
    endif()
elseif(TCMT_MACOS)
    # macOS executable settings
endif()

message(STATUS "TCMT-M build configured successfully")
