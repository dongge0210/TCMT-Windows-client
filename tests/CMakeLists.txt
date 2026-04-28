# Test infrastructure for TCMT Core
# Uses GoogleTest via CMake FetchContent

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

# Helper: create a test executable linked against TCMTCore
function(add_tcmt_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE
        TCMTCore
        GTest::gtest_main
        GTest::gmock_main
    )
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── IPC serialization tests ──
add_tcmt_test(test_ipc test_ipc.cpp)

# ── ConfigManager tests ──
add_tcmt_test(test_config test_config.cpp)
