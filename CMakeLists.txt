project(melon)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest) 

set(CMAKE_C_FLAGS_DEBUG "-DTZ_DEBUG")

## Third party libs
# glad 

add_library(glad STATIC
                 thirdparty/glad/src/glad.c
                 thirdparty/glad/include
)
include_directories(thirdparty/glad/include)

# GLFW

# Building only the GLFW lib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")

add_subdirectory(thirdparty/glfw)

find_package(OpenGL REQUIRED)

# TinyCThread
add_subdirectory(thirdparty/tinycthread)
include_directories(thirdparty/tinycthread/source)

# header only stuff
add_library(header_only_impls STATIC 
                              thirdparty/header_only_impls.c)
include_directories(thirdparty)

## libs
set(CMAKE_CXX_FLAGS_DEBUG  "-g")
set(CMAKE_CXX_STANDARD 11)

# engine lib
include_directories(include)

add_library(melon_core STATIC src/core.cpp)
target_link_libraries(melon_core tinycthread header_only_impls)
target_include_directories(melon_core PUBLIC include)

add_library(melon_gfx STATIC src/gfx.cpp)
target_link_libraries(melon_gfx melon_core ${OPENGL_LIBRARIES} glfw glad)
target_include_directories(melon_gfx PUBLIC include)

set(MELON_LIBS melon_core melon_gfx)

## tests
add_subdirectory(test)

## examples
add_subdirectory(examples)