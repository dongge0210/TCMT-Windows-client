# TCMT Windows Client - CMake构建系统
# 支持Windows和macOS平台，使用条件编译实现跨平台

cmake_minimum_required(VERSION 3.20)
project(TCMTClient
    VERSION 0.14.0
    DESCRIPTION "TCMT Windows Client - Cross-platform hardware monitoring tool"
    LANGUAGES CXX
)

# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出构建类型
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")

# 包含CMake模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
include(Platform)
include(Options)
include(Dependencies)

# 平台检测和配置
tcmt_detect_platform()

# 配置编译选项
tcmt_set_compile_options()

# 添加子目录
add_subdirectory(src)

# 安装配置（如果需要）
if(TCMT_INSTALL)
    install(TARGETS TCMTClient
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

    if(TCMT_WINDOWS)
        # Windows特定安装配置
        install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION .)
    elseif(TCMT_MACOS)
        # macOS特定安装配置
        install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION .)
    endif()
endif()

# 总结构建配置
message(STATUS "========================================")
message(STATUS "TCMT Client Build Configuration Summary")
message(STATUS "========================================")
message(STATUS "Platform: ${TCMT_PLATFORM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(TCMT_WINDOWS)
    message(STATUS "Windows SDK: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    if(MSVC)
        message(STATUS "MSVC工具集: v${MSVC_TOOLSET_VERSION}")
    endif()
endif()
message(STATUS "========================================")