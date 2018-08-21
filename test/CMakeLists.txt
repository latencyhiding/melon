add_executable(pool_test pool_test.t.cpp)
target_link_libraries(pool_test gtest gtest_main ${MELON_LIBS})
add_test(pool_test pool_test)
