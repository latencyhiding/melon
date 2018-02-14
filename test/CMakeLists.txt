enable_testing()

add_executable(pool_test pool_test.c)
target_link_libraries(pool_test glfw glad header_only_impls engine)
add_test(pool_test pool_test)
