# CMakeLists.txt for B_UI
cmake_minimum_required(VERSION 3.16)

# 如果这是主项目，设置项目名称
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SystemMonitorUI)
endif()

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# B_UI 静态库
add_library(B_UI STATIC
    ConsoleUI.cpp
    ConsoleUI.h
)

# 设置包含目录
target_include_directories(B_UI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接Windows库
if(WIN32)
    target_link_libraries(B_UI
        kernel32
        user32
    )
endif()

# 如果这是主项目，创建可执行文件
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 添加核心模块源文件
    file(GLOB_RECURSE CORE_SOURCES
        ../core/cpu/*.cpp
        ../core/gpu/*.cpp
        ../core/memory/*.cpp
        ../core/disk/*.cpp
        ../core/os/*.cpp
        ../core/temperature/*.cpp
        ../core/Utils/*.cpp
        ../core/DataStruct/*.cpp
        ../core/network/*.cpp
    )

    # 添加第三方库源文件（如果需要）
    file(GLOB_RECURSE THIRD_PARTY_SOURCES
        ../third_party/LibreHardwareMonitor/*.cpp
    )

    # 主UI程序
    add_executable(SystemMonitorUI
        ../main_ui.cpp
        ../main.cpp  # 包含后台监控函数
        ${CORE_SOURCES}
        ${THIRD_PARTY_SOURCES}
    )

    # 设置包含目录
    target_include_directories(SystemMonitorUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../core
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    )

    # 链接Windows库
    target_link_libraries(SystemMonitorUI
        B_UI
        kernel32
        user32
        ole32
        oleaut32
        wbemuuid
        pdh
        psapi
        iphlpapi
        ws2_32
        setupapi
        cfgmgr32
        advapi32
    )

    # 设置输出目录
    set_target_properties(SystemMonitorUI PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    )
endif()
