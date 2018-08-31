#ifdef MELON_USE_GLFW

#include "window_backend.h"
#include <melon/core/error.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

static void glfw_cursor_pos_cb(GLFWwindow* window, double x, double y)
{
    melon_input_event new_event = {
        .type = MELON_MOUSE_MOVED,
        .window_moved = {
            .x = (float) x,
            .y = (float) y
        }
    };

    MELON_LOG("cursor moved x: %f, y: %f", x, y);

    melon_push_input_event(&new_event);
}

static GLFWwindow* g_glfw_headless_window;

bool melon_window_backend_init()
{
    bool rc = glfwInit();
    
#ifdef MELON_USE_OPENGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#endif

    g_glfw_headless_window = glfwCreateWindow(1, 1, "", NULL, NULL);
    
    glfwMakeContextCurrent(g_glfw_headless_window);

#ifdef MELON_USE_OPENGL
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
#endif
 
    return rc;
}

void melon_window_backend_destroy()
{
    glfwDestroyWindow(g_glfw_headless_window);
}

melon_window* melon_create_window(int width, int height, const char* title) 
{
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    MELON_ASSERT(window, "Window creation error");

    if (g_glfw_headless_window == glfwGetCurrentContext())
    {
        glfwMakeContextCurrent(window);
    }    

    MELON_ASSERT(window == glfwGetCurrentContext());

    glfwSetCursorPosCallback(window, glfw_cursor_pos_cb);

    return (melon_window*) window;
}

void melon_destroy_window(melon_window* window) 
{
    glfwDestroyWindow((GLFWwindow*) window);
}

bool melon_window_should_close(melon_window* window)
{
    return glfwWindowShouldClose((GLFWwindow*) window);
}

void melon_poll_input_events() 
{
    glfwPollEvents();
}

void melon_swap_buffers(melon_window* window)
{
    glfwSwapBuffers((GLFWwindow*) window);
}

#endif