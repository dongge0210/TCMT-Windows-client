# TCMT Client核心库构建
# 第一阶段：基础CMake结构 - 建立框架

# 第一阶段目标：建立CMake基础结构，支持平台检测和编译选项配置
# 注意：完整的跨平台实现将在后续阶段完成

message(STATUS "Configuring TCMTClient build system (Phase 1)...")

# 第一阶段：设置基本目标结构
# 实际源文件将在后续阶段添加

# 创建库目标（将在后续阶段填充源文件）
add_library(TCMTCore STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/placeholder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Platform/Platform.cpp
)

# 设置目标属性
set_target_properties(TCMTCore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 设置包含目录
target_include_directories(TCMTCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# 平台特定配置
if(TCMT_WINDOWS)
    message(STATUS "  Platform: Windows - configuring Windows-specific settings")

    # Windows特定定义
    target_compile_definitions(TCMTCore PRIVATE
        TCMT_WINDOWS
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )

elseif(TCMT_MACOS)
    message(STATUS "  Platform: macOS - configuring macOS-specific settings")

    # macOS特定定义
    target_compile_definitions(TCMTCore PRIVATE
        TCMT_MACOS
        _DARWIN_C_SOURCE
    )

else()
    message(FATAL_ERROR "Unsupported platform detected")
endif()

# 应用通用编译选项
tcmt_target_set_options(TCMTCore)

# 应用平台库配置
tcmt_set_platform_libraries(TCMTCore)

# 设置输出目录
tcmt_set_output_directories(TCMTCore)

message(STATUS "TCMTCore library framework configured for Phase 1")

# 创建可执行文件目标（将在后续阶段填充源文件）
# 第一阶段包含main.cpp作为占位符，即使它是Windows特定的
# 在后续阶段将实现跨平台版本
if(TCMT_WINDOWS)
    add_executable(TCMTClient ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
else()
    # 对于非Windows平台，创建一个简单的占位符可执行文件
    add_executable(TCMTClient ${CMAKE_CURRENT_SOURCE_DIR}/core/placeholder.cpp)
endif()

# 链接核心库
target_link_libraries(TCMTClient PRIVATE TCMTCore)

# 设置可执行文件属性
set_target_properties(TCMTClient PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "TCMTClient executable framework configured for Phase 1")

# 注：实际源文件将在后续阶段添加
# 第一阶段仅建立CMake框架和平台检测