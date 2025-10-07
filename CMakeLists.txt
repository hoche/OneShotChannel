# CMakeLists.txt for YAML Parser Tests
# Minimum required CMake version
cmake_minimum_required(VERSION 3.14)

# Project name and version
project(OneShotFutureTests VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Fetch GoogleTest if not found
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Source files
set(TEST_SOURCES
    tests/oneshot_future_tests.cpp
    tests/oneshot_channel_tests.cpp
)

# Create the test executable
add_executable(oneshot_tests
    ${TEST_SOURCES})

# Link GoogleTest libraries
target_link_libraries(oneshot_tests
    gtest_main
    gtest
)

# Include directories (if needed, e.g., for the .hpp file)
target_include_directories(oneshot_tests
    PRIVATE include)

# Add the test
add_test(NAME oneshot_tests COMMAND oneshot_tests)

# Optional: Coverage (if using gcov/clang-cov)
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     include(CTest)
#     target_compile_options(oneshot_tests PRIVATE -fprofile-arcs -ftest-coverage)
#     target_link_options(oneshot_tests PRIVATE -fprofile-arcs)
# endif()
