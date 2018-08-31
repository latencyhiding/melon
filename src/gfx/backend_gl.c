#ifdef MELON_USE_OPENGL

#include <melon/gfx.h>
#include "gfx_commands.h"

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#define MELON_GL_INVALID_ID 0
#define MELON_GL_HANDLE(handle) ((GLuint) handle.data)

melon_gfx_handle melon_gfx_invalid_handle = {0};

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
        exit(1);
    }
    return errorCode;
}

static GLenum gl_melon_buffer_usage(melon_buffer_usage usage)
{
    switch (usage)
    {
        case MELON_STATIC_BUFFER: return GL_STATIC_DRAW;
        case MELON_DYNAMIC_BUFFER: return GL_DYNAMIC_DRAW;
        case MELON_STREAM_BUFFER: return GL_STREAM_DRAW;
        default: MELON_ASSERT("Buffer usage not supported\n");
    }
}

static GLenum gl_data_format(melon_vertex_data_type type)
{
    switch (type)
    {
        case MELON_FORMAT_BYTE: return GL_BYTE;
        case MELON_FORMAT_UBYTE: return GL_UNSIGNED_BYTE;
        case MELON_FORMAT_SHORT: return GL_SHORT;
        case MELON_FORMAT_USHORT: return GL_UNSIGNED_SHORT;
        case MELON_FORMAT_INT: return GL_INT;
        case MELON_FORMAT_UINT: return GL_UNSIGNED_INT;
        case MELON_FORMAT_HALF: return GL_HALF_FLOAT;
        case MELON_FORMAT_FLOAT: return GL_FLOAT;
        default: MELON_ASSERT(false, "Data format not supported\n");
    }
}

typedef struct
{
    int    location;
    size_t buffer_binding;
    size_t offset;
    GLenum data_type;
    int    size;
    int    divisor;
} vertex_attrib_gl;

typedef struct
{
    melon_shader_handle shader_program;
    vertex_attrib_gl  attribs[MELON_GFX_MAX_ATTRIBUTES];
    size_t            num_attribs;
    size_t            stride;
} pipeline_gl;

MELON_HANDLE_MAP_TYPEDEF(pipeline_gl)
MELON_HANDLE_MAP_TYPEDEF(cb_command_buffer)

typedef struct
{
    melon_map_pipeline_gl       pipelines;
    melon_map_cb_command_buffer command_buffers;
    GLuint                              dummy_vao;

    melon_device_params config;
} device_gl;

static device_gl g_device;

MELON_GFX_CREATE_DEVICE(melon_gfx_backend_init)
{
    if (!device_config)
    {
        g_device.config = *(melon_default_device_params());
    }
    else 
    {
        g_device.config = *device_config;
    }

    melon_create_map(&g_device.pipelines, g_device.config.resource_count.max_pipelines,
                             &g_device.config.allocator, false);
    melon_create_map(&g_device.command_buffers, g_device.config.resource_count.max_command_buffers,
                             &g_device.config.allocator, false);

    g_device.dummy_vao = 0;
    return true;
}

MELON_GFX_DELETE_DEVICE(melon_gfx_backend_destroy) { glDeleteVertexArrays(1, &g_device.dummy_vao); }

static GLuint compile_shader(const melon_allocator_api* allocator, GLenum type,
                             const melon_shader_stage_params* shader_stage_create_info)
{
    if (!shader_stage_create_info->source)
        return 0;

    // Create shader
    GLuint shader_stage = glCreateShader(type);
    // Compile shader
    glShaderSource(shader_stage, 1, &shader_stage_create_info->source, (GLint*) &shader_stage_create_info->size);
    // Compile the vertex shader
    glCompileShader(shader_stage);

    // Check for errors
    GLint compiled = 0;
    glGetShaderiv(shader_stage, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
    {
        GLint len = 0;
        glGetShaderiv(shader_stage, GL_INFO_LOG_LENGTH, &len);

        char* error_buffer = (char*) MELON_ALLOC((*allocator), sizeof(char) * len, MELON_DEFAULT_ALIGN);
        glGetShaderInfoLog(shader_stage, len, &len, error_buffer);

        MELON_LOG("Shader compilation error in %s: %s\n", shader_stage_create_info->name, error_buffer);

        MELON_FREE((*allocator), error_buffer);
        glDeleteShader(shader_stage);

        // Return invalid id if error
        return 0;
    }

    // Add to pool
    return shader_stage;
}

MELON_GFX_CREATE_SHADER(melon_create_shader)
{
    melon_shader_handle shader_id = { MELON_GL_INVALID_ID };

    GLuint vertex_shader
        = compile_shader(&g_device.config.allocator, GL_VERTEX_SHADER, &shader_create_info->vertex_shader);
    GLuint fragment_shader
        = compile_shader(&g_device.config.allocator, GL_FRAGMENT_SHADER, &shader_create_info->fragment_shader);

    if (!vertex_shader || !fragment_shader)
    {
        return shader_id;
    }

    GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int*) &linked);
    if (linked == GL_FALSE)
    {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        char* error_buffer = (char*) MELON_ALLOC(g_device.config.allocator, sizeof(char) * len, MELON_DEFAULT_ALIGN);
        glGetProgramInfoLog(program, len, &len, error_buffer);

        MELON_LOG("Shader linking error: %s\n", error_buffer);

        MELON_FREE(g_device.config.allocator, error_buffer);
        glDeleteProgram(program);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);

        // Return invalid id if error
        return shader_id;
    }

    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    MELON_LOG("Shader successfully compiled and linked using %s and %s\n", shader_create_info->vertex_shader.name,
              shader_create_info->fragment_shader.name);

    shader_id = (melon_shader_handle) { program };

    return shader_id;
}

MELON_GFX_DELETE_SHADER(melon_delete_shader) { glDeleteProgram((GLuint) shader.data); }

MELON_GFX_CREATE_BUFFER(melon_create_buffer)
{
    melon_buffer_handle buffer_id = { MELON_GL_INVALID_ID };

    GLuint buf     = 0;
    GLenum binding = GL_ARRAY_BUFFER;    // This is set to GL_ARRAY_BUFFER because at this stage the binding
                                         // doesn't matter. This is just to get the buffer bound.
    GLuint usage = gl_melon_buffer_usage(buffer_create_info->usage);

    glGenBuffers(1, &buf);
    glBindBuffer(binding, buf);
    glBufferData(binding, buffer_create_info->size, buffer_create_info->data, usage);

    if (!buf)
    {
        MELON_LOG("Buffer creation error: could not create OpenGL buffer\n");
        return buffer_id;
    }

    buffer_id = (melon_buffer_handle) { buf };

    glBindBuffer(binding, 0);

    return buffer_id;
}

MELON_GFX_DELETE_BUFFER(melon_delete_buffer)
{
    GLuint handle = (GLuint) buffer.data;
    glDeleteBuffers(1, &handle);
}

MELON_GFX_CREATE_PIPELINE(melon_create_pipeline)
{
    pipeline_gl new_pipeline    = { 0 };
    new_pipeline.shader_program = pipeline_create_info->shader_program;

    // TODO: Pipeline validation, dummy draw call
    GLuint gl_program = MELON_GL_HANDLE(new_pipeline.shader_program);

    size_t packed_stride = 0;
    for (int attrib_index = 0, gl_attrib_index = 0;
         attrib_index < MELON_GFX_MAX_ATTRIBUTES && gl_attrib_index < MELON_GFX_MAX_ATTRIBUTES;
         attrib_index++, gl_attrib_index++)
    {
        const melon_vertex_attrib_params* attrib_params = pipeline_create_info->vertex_attribs + attrib_index;
        if (attrib_params->type == MELON_FORMAT_INVALID)
            continue;

        if (attrib_params->buffer_binding > MELON_GFX_MAX_BUFFER_ATTACHMENTS)
        {
            gl_attrib_index--;
            continue;
        }

        vertex_attrib_gl new_attrib;
        new_attrib.data_type      = gl_data_format(attrib_params->type);
        new_attrib.divisor        = attrib_params->divisor;
        new_attrib.offset         = attrib_params->offset;
        new_attrib.size           = attrib_params->size;
        new_attrib.buffer_binding = attrib_params->buffer_binding;

        if (attrib_params->name)
            new_attrib.location = glGetAttribLocation(gl_program, attrib_params->name);
        else
            new_attrib.location = attrib_index;

        new_pipeline.attribs[gl_attrib_index] = new_attrib;
        new_pipeline.num_attribs++;

        packed_stride += new_attrib.size * melon_vertex_data_type_bytes(attrib_params->type);
    }

    if (pipeline_create_info->stride == 0)
    {
        new_pipeline.stride = packed_stride;
    }
    else
    {
        new_pipeline.stride = pipeline_create_info->stride;
    }
    return (melon_pipeline_handle) { melon_map_push(&g_device.pipelines, &new_pipeline) };
}

MELON_GFX_DELETE_PIPELINE(melon_delete_pipeline)
{
    if (!melon_map_delete(&g_device.pipelines, pipeline.data))
    {
        MELON_LOG("Pipeline deletion error: invalid ID.\n");
    }
}

static void gl3_clear_pipeline(melon_pipeline_handle pipeline_id)
{
    pipeline_gl* pipeline_gl = melon_map_get(&g_device.pipelines, pipeline_id.data);

    for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
    {
        vertex_attrib_gl* attrib = &pipeline_gl->attribs[attrib_index];
        glDisableVertexAttribArray(attrib->location);
    }

    glUseProgram(0);
}

static void gl3_melon_cmd_bind_pipeline(melon_draw_state*           current_melon_draw_state,
                                        const melon_pipeline_handle pipeline_id)
{
    pipeline_gl* pipeline_gl           = melon_map_get(&g_device.pipelines, pipeline_id.data);
    current_melon_draw_state->pipeline = pipeline_id;
    gl3_clear_pipeline(pipeline_id);

    MELON_ASSERT(MELON_GFX_HANDLE_IS_VALID(pipeline_gl->shader_program),
                 "Pipeline creation error: shader program ID invalid.");
    GLuint shader_program = MELON_GL_HANDLE(pipeline_gl->shader_program);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(shader_program);
}

static void gl3_bind_resources(melon_pipeline_handle pipeline_id, melon_draw_state* current_melon_draw_state,
                               const melon_draw_resources* melon_draw_resources)
{
    pipeline_gl* pipeline_gl = melon_map_get(&g_device.pipelines, pipeline_id.data);

    for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
    {
        GLuint            current_buffer = 0;
        vertex_attrib_gl* attrib         = &pipeline_gl->attribs[attrib_index];

        melon_buffer_handle buffer = melon_draw_resources->buffers[attrib->buffer_binding];
        MELON_ASSERT(MELON_GFX_HANDLE_IS_VALID(buffer), "Buffer at binding %lu was invalid", attrib->buffer_binding);

        if (current_melon_draw_state->resources.buffers[attrib->buffer_binding].data == buffer.data)
            continue;

        current_melon_draw_state->resources.buffers[attrib->buffer_binding] = buffer;

        if (current_buffer != MELON_GL_HANDLE(buffer))
        {
            current_buffer = MELON_GL_HANDLE(buffer);
            glBindBuffer(GL_ARRAY_BUFFER, current_buffer);
        }

        glVertexAttribPointer(attrib->location, attrib->size, attrib->data_type, GL_FALSE, pipeline_gl->stride,
                              (GLvoid*) attrib->offset);
        glVertexAttribDivisor(attrib->location, attrib->divisor);
        glEnableVertexAttribArray(attrib->location);
    }

    if (current_melon_draw_state->resources.index_buffer.data != melon_draw_resources->index_buffer.data
        && MELON_GFX_HANDLE_IS_VALID(current_melon_draw_state->resources.index_buffer))
    {
        current_melon_draw_state->resources.index_buffer = melon_draw_resources->index_buffer;
        current_melon_draw_state->resources.index_type   = melon_draw_resources->index_type;

        GLuint index_buffer = MELON_GL_HANDLE(current_melon_draw_state->resources.index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    }
}

static GLenum gl_melon_draw_type(melon_draw_type type)
{
    switch (type)
    {
        case MELON_TRIANGLES: return GL_TRIANGLES;
        case MELON_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
        case MELON_LINES: return GL_LINE;
        case MELON_POINTS: return GL_POINT;
    }
}

// TODO: should pass in a "command context" object to store current state, eventually wrap
// everything in a command buffer
MELON_GFX_EXECUTE_DRAW_GROUPS(melon_execute_draw_groups)
{
    if (g_device.dummy_vao == 0)
    {
        glGenVertexArrays(1, &g_device.dummy_vao);
        glBindVertexArray(g_device.dummy_vao);
    }

    melon_draw_state current_melon_draw_state = { 0 };
    for (size_t i = 0; i < num_melon_draw_groups; i++)
    {
        melon_pipeline_handle       pipeline  = melon_draw_groups[i].pipeline;
        const melon_draw_resources* resources = &melon_draw_groups[i].resources;

        MELON_ASSERT(melon_map_handle_is_valid(&g_device.pipelines, pipeline.data),
                     "Pipeline binding error: pipeline ID invalid.");

        gl3_melon_cmd_bind_pipeline(&current_melon_draw_state, pipeline);

        const melon_draw_group* melon_draw_group = &melon_draw_groups[i];
        for (size_t j = 0; j < melon_draw_group->num_draw_calls; j++)
        {
            const melon_draw_call_params* draw_call = &(melon_draw_group->draw_calls[j]);
            gl3_bind_resources(pipeline, &current_melon_draw_state, resources);
            glCheckError();

            if (MELON_GFX_HANDLE_IS_VALID(resources->index_buffer))
            {
                glDrawElementsInstancedBaseVertex(gl_melon_draw_type(draw_call->type), draw_call->num_vertices,
                                                  gl_data_format(resources->index_type), NULL, draw_call->instances,
                                                  draw_call->base_vertex);
            }
            else
            {
                glDrawArraysInstanced(gl_melon_draw_type(draw_call->type), draw_call->base_vertex,
                                      draw_call->num_vertices, draw_call->instances);
            }
        }
    }
    gl3_clear_pipeline(current_melon_draw_state.pipeline);
}

MELON_GFX_CREATE_COMMAND_BUFFER(melon_create_command_buffer)
{
    cb_command_buffer new_cb;
    cb_create(&g_device.config.allocator, &new_cb, MELON_MEGABYTE(2));
    return (melon_command_buffer_handle){ melon_map_push(&g_device.command_buffers, &new_cb) };
}

MELON_GFX_DELETE_COMMAND_BUFFER(melon_delete_command_buffer)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_destroy(p);
}

MELON_GFX_CB_BEGIN_RECORDING(melon_begin_recording)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_begin_recording(p);
}

MELON_GFX_CB_END_RECORDING(melon_end_recording)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_end_recording(p);
}

MELON_GFX_CB_BIND_VERTEX_BUFFER(melon_cmd_bind_vertex_buffer)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_cmd_bind_vertex_buffer(p, buffer, binding);
}

MELON_GFX_CB_BIND_INDEX_BUFFER(melon_cmd_bind_index_buffer)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_cmd_bind_index_buffer(p, buffer);
}

MELON_GFX_CB_BIND_PIPELINE(melon_cmd_bind_pipeline)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_cmd_bind_pipeline(p, pipeline);
}

MELON_GFX_CB_DRAW(melon_cmd_draw)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_cmd_draw(p, params);
}

MELON_GFX_CB_RESET(melon_reset)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_reset(p);
}

void melon_begin_consuming(melon_command_buffer_handle cb)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_begin_consuming(p);
}

void melon_end_consuming(melon_command_buffer_handle cb)
{
    cb_command_buffer* p = melon_map_get(&g_device.command_buffers, cb.data);
    cb_end_consuming(p);
}

/**
 * TODO: Render passes, sorting commands by render pass
 */
MELON_GFX_CB_SUBMIT(submit_commands)
{
    if (g_device.dummy_vao == 0)
    {
        glGenVertexArrays(1, &g_device.dummy_vao);
        glBindVertexArray(g_device.dummy_vao);
    }
    // Translate to GL
    for (size_t i = 0; i < num_cbs; i++)
    {
        cb_command_buffer* p = melon_map_get(&g_device.command_buffers, command_buffers[i].data);
        cb_begin_consuming(p);

        cb_command* cmd = cb_pop_command(p);
        while (cmd)
        {
            switch (cmd->type)
            {
                case MELON_CMD_BIND_VERTEX_BUFFER:
                {
                    cb_cmd_bind_vertex_buffer_data* bind_data = (cb_cmd_bind_vertex_buffer_data*) cmd->data;
                    // TODO
                    break;
                }
                case MELON_CMD_BIND_INDEX_BUFFER:
                {
                    melon_buffer_handle* ib = (melon_buffer_handle*) cmd->data;
                    // TODO
                    break;
                }
                case MELON_CMD_BIND_PIPELINE:
                {
                    melon_pipeline_handle* pipeline = (melon_pipeline_handle*) cmd->data;
                    // TODO
                    break;
                }
                case MELON_CMD_DRAW:
                {
                    melon_draw_call_params* params = (melon_draw_call_params*) cmd->data;
                    // TODO
                    break;
                }
            }

            cmd = cb_pop_command(p);
        }

        cb_end_consuming(p);
    }
}

#endif