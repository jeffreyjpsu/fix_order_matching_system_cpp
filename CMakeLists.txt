cmake_minimum_required(VERSION 3.10)
project(fix_order_matching_system_cpp 
    VERSION 1.0.0
    LANGUAGES CXX)

# Use C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set global compiler flags
if(MSVC)
    add_compile_options(/W3 /EHsc)
else()
    add_compile_options(-Wall -Wextra -pthread)
endif()

# Dependencies Definitions
set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")

# Boost
set(BOOST_ROOT "${DEPS_DIR}/boost")
include_directories(${BOOST_ROOT})

# QuickFIX
set(QUICKFIX_INCLUDE_DIR "${DEPS_DIR}/install/usr/local/include")
set(QUICKFIX_LIBRARY_DIR "${DEPS_DIR}/quickfix/lib")
include_directories(${QUICKFIX_INCLUDE_DIR})
link_directories(${QUICKFIX_LIBRARY_DIR})
# On macOS, QuickFIX assumes .dylib by default, but we need to ensure we link correctly
find_library(QUICKFIX_LIB NAMES quickfix PATHS ${QUICKFIX_LIBRARY_DIR} NO_DEFAULT_PATH)

# Source Configuration
include_directories(source)

# Collect Source Files
file(GLOB_RECURSE SOURCES 
    "source/concurrent/*.cpp"
    "source/memory/*.cpp"
    "source/order_matcher/*.cpp"
    "source/server/*.cpp"
    "source/utility/*.cpp"
)

# Main Server Executable
add_executable(order_matching_server source/server_main.cpp ${SOURCES})
target_link_libraries(order_matching_server PRIVATE ${QUICKFIX_LIB} pthread)

# Unit Tests
enable_testing()

set(GTEST_ROOT "${DEPS_DIR}/gtest-1.7.0")
include_directories(${GTEST_ROOT} "${GTEST_ROOT}/include")

# GTest Source (compile directly as part of test target or separate lib)
set(GTEST_SOURCES 
    "${GTEST_ROOT}/src/gtest-all.cc"
    "${GTEST_ROOT}/src/gtest_main.cc"
)

add_executable(unit_tests 
    test_unit/test.cpp
    ${SOURCES}
    ${GTEST_SOURCES}
)

target_compile_definitions(unit_tests PRIVATE UNIT_TEST)
target_link_libraries(unit_tests PRIVATE pthread)

# RPATH for macOS
if(APPLE)
    set_target_properties(order_matching_server PROPERTIES BUILD_RPATH "${QUICKFIX_LIBRARY_DIR}")
    set_target_properties(unit_tests PROPERTIES BUILD_RPATH "${QUICKFIX_LIBRARY_DIR}")
endif()
