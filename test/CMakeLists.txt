set(LIBS glfw glad header_only_impls engine)

function(create_unit_test TEST_NAME TEST_SOURCES)
  add_executable(${TEST_NAME} ${TEST_SOURCES})
  target_link_libraries(${TEST_NAME} ${LIBS})
  add_test(${TEST_NAME} ${TEST_NAME})
endfunction(create_unit_test)

create_unit_test(pool_test pool_test.c)
