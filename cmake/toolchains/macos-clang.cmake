# macOS Clang Toolchain File

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify compiler
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Set standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compilation options
add_compile_options(
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-DNDEBUG>
    -Wall
    -Wextra
    -Wpedantic
)

# Preprocessor definitions
add_compile_definitions(
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    PLATFORM_MACOS
)

# macOS specific settings
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MACOSX_RPATH ON)

# Find system frameworks
find_library(CORE_FOUNDATION_LIB CoreFoundation REQUIRED)
find_library(IOKIT_LIB IOKit REQUIRED)
find_library(SYSTEM_CONFIGURATION_LIB SystemConfiguration REQUIRED)
find_library(FOUNDATION_LIB Foundation REQUIRED)

# Link options
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")

# Output directory settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)