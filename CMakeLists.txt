cmake_minimum_required(VERSION 3.14)
project(Fixed128 VERSION 1.7.0 LANGUAGES CXX)

add_library(fixed128 INTERFACE)
target_include_directories(fixed128 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fixed128 INTERFACE cxx_std_14)

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

option(FIXED128_BUILD_TESTS "Build Fixed128 tests" ON)
if(FIXED128_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS fixed128 EXPORT fixed128Targets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT fixed128Targets
    FILE fixed128Targets.cmake
    NAMESPACE fixed128::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fixed128)

foreach(folder IN ITEMS include tests)
  foreach(extension IN ITEMS hpp h cpp c cc)
    list(APPEND GLOB_EXPRESSIONS "${folder}/*.${extension}")
  endforeach()
endforeach()

file(GLOB_RECURSE ALL_SOURCES ${GLOB_EXPRESSIONS})
add_custom_target(format COMMAND clang-format -i ${ALL_SOURCES})
