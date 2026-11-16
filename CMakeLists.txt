cmake_minimum_required(VERSION 3.14)
project(fixed128 VERSION 1.2.0 LANGUAGES CXX)

# Header-only INTERFACE library
add_library(fixed128 INTERFACE)
target_include_directories(fixed128 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fixed128 INTERFACE cxx_std_17)

# Tests
option(FIXED128_BUILD_TESTS "Build fixed128 tests" ON)
if(FIXED128_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS fixed128 EXPORT fixed128Targets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT fixed128Targets
    FILE fixed128Targets.cmake
    NAMESPACE fixed128::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fixed128)
