cmake_minimum_required(VERSION 3.14)
project(Fixed128 VERSION 1.6.0 LANGUAGES CXX)

# Header-only INTERFACE library
add_library(fixed128 INTERFACE)
target_include_directories(fixed128 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fixed128 INTERFACE cxx_std_14)

# Detect whether the compiler has native __int128. If not (MSVC), we need
# Boost.Multiprecision headers for the MSVC backend.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    int main() {
        __int128 x = 0;
        __int128 y = x + x;
        (void)y;
        return 0;
    }
" FIXED128_HAS_NATIVE_INT128)

if(NOT FIXED128_HAS_NATIVE_INT128)
    find_package(Boost REQUIRED)
    target_include_directories(fixed128 INTERFACE ${Boost_INCLUDE_DIRS})
endif()

# Tests
option(FIXED128_BUILD_TESTS "Build Fixed128 tests" ON)
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
