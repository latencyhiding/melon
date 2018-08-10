add_subdirectory(googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(pool_test pool_test.t.cpp)
target_link_libraries(pool_test gtest gtest_main melon)
add_test(pool_test pool_test)
