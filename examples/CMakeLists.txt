add_executable(triangle triangle/main.c)
target_compile_features(triangle PRIVATE c_std_99)
target_compile_definitions(triangle PRIVATE $<$<CONFIG:DEBUG>:MELON_DEBUG>)
target_link_libraries(triangle melon_gfx)