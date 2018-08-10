function(create_example EXAMPLE_NAME EXAMPLE_SOURCES)
  add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCES})
  target_link_libraries(${EXAMPLE_NAME} ${LIBS})
endfunction(create_example)

create_example(triangle triangle/main.cpp)