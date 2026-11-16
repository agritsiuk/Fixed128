include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(fixed128_tests test_fixed128.cpp)
target_link_libraries(fixed128_tests PRIVATE fixed128 GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(fixed128_tests)
