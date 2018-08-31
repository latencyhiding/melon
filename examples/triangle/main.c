#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <melon/core/error.h>
#include <melon/gfx.h>

static GLenum glCheckError()
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
    if (!melon_gfx_init(NULL))
    {
        exit(EXIT_FAILURE);
    }

    melon_window* window = melon_create_window(WIDTH, HEIGHT, "triangle");
    if (!window)
    {
        melon_gfx_destroy();
        exit(EXIT_FAILURE);
    }

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

    melon_shader_params melon_shader_params = {0};
    {
        melon_shader_params.vertex_shader.name   = "passthrough.vert";
        melon_shader_params.vertex_shader.source = vertex_source;
        melon_shader_params.vertex_shader.size   = strlen(vertex_source);
    }
    {
        melon_shader_params.fragment_shader.name   = "passthrough.frag";
        melon_shader_params.fragment_shader.source = fragment_source;
        melon_shader_params.fragment_shader.size   = strlen(fragment_source);
    }

    melon_shader_handle shader_program = melon_create_shader(&melon_shader_params);

    float vertices[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };

    melon_buffer_params melon_buffer_params = {0};
    {
        melon_buffer_params.data  = vertices;
        melon_buffer_params.size  = sizeof(vertices);
        melon_buffer_params.usage = MELON_STATIC_BUFFER;
    }
    melon_buffer_handle vertex_buffer = melon_create_buffer(&melon_buffer_params);

    melon_pipeline_params melon_pipeline_params = {0};
    {
        melon_vertex_attrib_params position_attrib = {0};

        position_attrib.name           = "position";
        position_attrib.buffer_binding = 0;
        position_attrib.offset         = 0;
        position_attrib.type           = MELON_FORMAT_FLOAT;
        position_attrib.size           = 2;
        position_attrib.divisor        = 0;

        melon_pipeline_params.vertex_attribs[0] = position_attrib;

        melon_pipeline_params.shader_program = shader_program;
    }

    melon_pipeline_handle pipeline = melon_create_pipeline(&melon_pipeline_params);

    melon_draw_call_params draw_calls = {0};
    {
        draw_calls.type         = MELON_TRIANGLES;
        draw_calls.instances    = 1;
        draw_calls.base_vertex  = 0;
        draw_calls.num_vertices = 3;
    }

    melon_draw_group melon_draw_group = {0};
    {
        melon_draw_group.pipeline  = pipeline;
        {
            melon_draw_group.resources.buffers[0] = vertex_buffer;
        }
        melon_draw_group.draw_calls     = &draw_calls;
        melon_draw_group.num_draw_calls = 1;
    }
    
    glClearColor(0, 0, 0, 0);
    while (!melon_window_should_close(window))
    {
        melon_poll_input_events();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        melon_execute_draw_groups(&melon_draw_group, 1);
        glCheckError();

        melon_swap_buffers(window);
    }

    melon_delete_shader(shader_program);
    melon_delete_pipeline(pipeline);

    melon_gfx_destroy();

    return 0;
}
