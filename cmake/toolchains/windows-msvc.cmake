# Windows MSVC Toolchain File

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x64)

# Specify compiler
set(CMAKE_C_COMPILER cl.exe)
set(CMAKE_CXX_COMPILER cl.exe)

# Set architecture
set(CMAKE_GENERATOR_PLATFORM x64)

# MSVC specific settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compilation options
add_compile_options(
    $<$<CONFIG:Debug>:/Od>
    $<$<CONFIG:Debug>:/Zi>
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/DNDEBUG>
    /Zc:__cplusplus
    /wd4005
    /wd4244
)

# C++/CLI support
add_compile_options(/clr)

# Preprocessor definitions
add_compile_definitions(
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    PLATFORM_WINDOWS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    _SCL_SECURE_NO_WARNINGS
)

# CUDA support
set(CUDA_PATH "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6" CACHE PATH "CUDA Installation Path")
if(EXISTS ${CUDA_PATH})
    set(CMAKE_CUDA_COMPILER ${CUDA_PATH}/bin/nvcc.exe)
    set(CUDAToolkit_ROOT ${CUDA_PATH})
    enable_language(CUDA)
endif()

# Windows SDK lookup
set(WINDOWS_SDK_PATH "C:/Program Files (x86)/Windows Kits/10/")
if(EXISTS ${WINDOWS_SDK_PATH})
    set(CMAKE_SYSTEM_PREFIX_PATH ${WINDOWS_SDK_PATH})
endif()

# Link library settings
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/RELEASE")

# .NET support
set(CMAKE_VS_WINRT_BY_DEFAULT OFF)

# Output directory settings
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)