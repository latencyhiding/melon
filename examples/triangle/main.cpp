#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <melon/core/error.h>
#include <melon/gfx.h>

static void error_callback(int error, const char* description) { fprintf(stderr, "%s", description); }

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

static const char* load_text_file(const char* filename)
{
    FILE*  f    = fopen(filename, "r");
    size_t size = 0;
    char*  data = NULL;

    if (!f)
    {
        MELON_LOG("Can't find file: %s\n", filename);
        return NULL;
    }

    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    rewind(f);

    data = (char*) malloc(size + 1);
    memset(data, 0, size + 1);

    fread(data, 1, size, f);
    fclose(f);

    return data;
}

GLenum glCheckError()
{
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        const char* error;
        switch (errorCode)
        {
            case GL_INVALID_ENUM: error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE: error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW: error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW: error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY: error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        printf("%s\n", error);
    }
    return errorCode;
}

#define WIDTH 800
#define HEIGHT 600

int main(int argc, char** argv)
{
    GLFWwindow* window;
    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    glfwSetKeyCallback(window, key_callback);

    melon::gfx::init();

    const char* vertex_source
        = "#version 330\n"
          "layout(location = 0) in vec2 position;"
          "void main()"
          "{"
          "    gl_Position = vec4(position, 0.0f, 1.0f);"
          "}";
    const char* fragment_source
        = "#version 330\n"
          "out vec4 out_color;"
          "void main()"
          "{"
          "    out_color = vec4(0.0f, 0.5f, 0.2f, 1.0f);"
          "}";

    melon::gfx::shader_params shader_params = {};
    {
        shader_params.vertex_shader.name   = "passthrough.vert";
        shader_params.vertex_shader.source = vertex_source;
        shader_params.vertex_shader.size   = strlen(vertex_source);
    }
    {
        shader_params.fragment_shader.name   = "passthrough.frag";
        shader_params.fragment_shader.source = fragment_source;
        shader_params.fragment_shader.size   = strlen(fragment_source);
    }

    melon::gfx::shader_handle shader_program = melon::gfx::create_shader(&shader_params);

    float vertices[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };

    melon::gfx::buffer_params buffer_params = {};
    {
        buffer_params.data  = vertices;
        buffer_params.size  = sizeof(vertices);
        buffer_params.usage = melon::gfx::MELON_STATIC_BUFFER;
    }
    melon::gfx::buffer_handle vertex_buffer = melon::gfx::create_buffer(&buffer_params);

    melon::gfx::pipeline_params pipeline_params = {};
    {
        melon::gfx::vertex_attrib_params position_attrib = {};

        position_attrib                = {};
        position_attrib.name           = "position";
        position_attrib.buffer_binding = 0;
        position_attrib.offset         = 0;
        position_attrib.type           = melon::gfx::MELON_FORMAT_FLOAT;
        position_attrib.size           = 2;
        position_attrib.divisor        = 0;

        pipeline_params.vertex_attribs[0] = position_attrib;

        pipeline_params.shader_program = shader_program;
    }

    melon::gfx::pipeline_handle pipeline = melon::gfx::create_pipeline(&pipeline_params);

    melon::gfx::draw_call_params draw_calls = {};
    {
        draw_calls.type         = melon::gfx::MELON_TRIANGLES;
        draw_calls.instances    = 1;
        draw_calls.base_vertex  = 0;
        draw_calls.num_vertices = 3;
    }

    melon::gfx::draw_group draw_group = {};
    {
        draw_group.pipeline  = pipeline;
        draw_group.resources = {};
        {
            draw_group.resources.buffers[0] = vertex_buffer;
        }
        draw_group.draw_calls     = &draw_calls;
        draw_group.num_draw_calls = 1;
    }

    glClearColor(0, 0, 0, 0);
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        melon::gfx::execute_draw_groups(&draw_group, 1);
        glCheckError();

        glfwSwapBuffers(window);
    }

    melon::gfx::delete_shader(shader_program);
    melon::gfx::delete_pipeline(pipeline);

    melon::gfx::destroy();

    return 0;
}
