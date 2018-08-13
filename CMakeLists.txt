cmake_minimum_required(VERSION 3.10)
project(melon)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################################################################
## In source libs
################################################################################

# glad 

add_library(glad STATIC
                 thirdparty/glad/src/glad.c
                 thirdparty/glad/include
)
target_include_directories(glad PUBLIC thirdparty/glad/include)

# GLFW

# Building only the GLFW lib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(GLFW_BUILD_EXAMPLES ON CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")

add_subdirectory(thirdparty/glfw)

find_package(OpenGL REQUIRED)

# TinyCThread
add_subdirectory(thirdparty/tinycthread)
target_include_directories(tinycthread PUBLIC thirdparty/tinycthread)

# header only stuff
add_library(header_only_impls thirdparty/header_only_impls.c)
target_include_directories(header_only_impls PUBLIC thirdparty)

## libs
## engine lib



# core
file(GLOB CORE_SOURCES "src/core/*.cpp")
add_library(melon_core ${CORE_SOURCES})
target_link_libraries(melon_core tinycthread header_only_impls)
target_compile_definitions(melon_core PRIVATE MELON_USE_OPENGL $<$<CONFIG:DEBUG>:MELON_DEBUG>)
target_include_directories(melon_core 
                           PUBLIC include/core
                                  include)

# gfx
file(GLOB GFX_SOURCES "src/gfx/*.cpp")
add_library(melon_gfx ${GFX_SOURCES})
if (MELON_USE_OPENGL)
    target_compile_definitions(melon_gfx PRIVATE MELON_USE_OPENGL $<$<CONFIG:DEBUG>:MELON_DEBUG>)
    target_link_libraries(melon_gfx melon_core ${OPENGL_LIBRARIES} glfw glad)
else()
    target_compile_definitions(melon_gfx PRIVATE MELON_USE_OPENGL $<$<CONFIG:DEBUG>:MELON_DEBUG>)
    target_link_libraries(melon_gfx melon_core ${OPENGL_LIBRARIES} glfw glad)
endif()

target_include_directories(melon_gfx 
                           PUBLIC include/gfx)

set(MELON_LIBS melon_core melon_gfx)

## tests
add_subdirectory(thirdparty/googletest)
add_subdirectory(test)

## examples
add_subdirectory(examples)