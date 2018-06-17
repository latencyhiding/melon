cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(APP_NAME graphics_test)
project(${APP_NAME} C)

set(SOURCES src/main.c)

set(CMAKE_C_FLAGS_DEBUG "-DTZ_DEBUG")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin )

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
include_directories(lib)
# engine lib
set(LIB_NAME engine)
add_library(${LIB_NAME} STATIC
                        lib/engine/tz_core.c
                        lib/engine/tz_graphics_backend.c)
set_property(TARGET ${LIB_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${LIB_NAME} PROPERTY C_STANDARD_REQUIRED ON)

## tests
enable_testing()
add_subdirectory(test)

##

add_executable(${APP_NAME} ${SOURCES})
set_property(TARGET ${APP_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${APP_NAME} PROPERTY C_STANDARD_REQUIRED ON)

target_link_libraries(${APP_NAME} ${OPENGL_LIBRARIES} engine glfw glad tinycthread header_only_impls)
