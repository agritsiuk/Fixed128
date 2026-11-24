include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

if(FIXED128_HAS_NATIVE_INT128 AND NOT FIXED128_FORCE_LITE_BACKEND)
    add_executable(fixed128_tests test_fixed128.cpp)
    target_link_libraries(fixed128_tests PRIVATE fixed128 GTest::gtest_main)
    gtest_discover_tests(fixed128_tests)
endif()

add_executable(fixed128_lite_tests test_fixed128_lite.cpp)
target_link_libraries(fixed128_lite_tests PRIVATE fixed128 GTest::gtest_main)
gtest_discover_tests(fixed128_lite_tests)
