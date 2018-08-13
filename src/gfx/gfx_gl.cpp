#ifdef MELON_USE_OPENGL

#include <melon/gfx.h>
#include "gfx_commands.h"

namespace melon
{

namespace gfx
{

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#define MELON_GL_INVALID_ID 0
#define MELON_GL_HANDLE(handle) ((GLuint) handle.data)

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
    shader_handle    shader_program;
    vertex_attrib_gl attribs[MELON_MAX_ATTRIBUTES];
    size_t           num_attribs;
    size_t           stride;
} pipeline_gl;

MELON_POOL_VECTOR(gl_pipeline, pipeline_gl)
MELON_POOL_VECTOR(command_buffer, commands::command_buffer)

typedef struct
{
    gl_pipeline_pool    pipelines;
    command_buffer_pool command_buffers;
    GLuint              dummy_vao;

    device_params config;
} device_gl;

static device_gl g_device;

static GLenum gl_buffer_usage(buffer_usage usage)
{
    switch (usage)
    {
        case MELON_STATIC_BUFFER: return GL_STATIC_DRAW;
        case MELON_DYNAMIC_BUFFER: return GL_DYNAMIC_DRAW;
        case MELON_STREAM_BUFFER: return GL_STREAM_DRAW;
        default: MELON_ASSERT("Buffer usage not supported\n");
    }
}

static GLenum gl_data_format(vertex_data_type type)
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

MELON_GFX_CREATE_DEVICE(init)
{
    g_device.config = *device_config;

    create_gl_pipeline_pool(&g_device.pipelines, g_device.config.resource_count.max_pipelines,
                            &g_device.config.allocator);
    g_device.dummy_vao = 0;

    glGenVertexArrays(1, &g_device.dummy_vao);
    glBindVertexArray(g_device.dummy_vao);
}

MELON_GFX_DELETE_DEVICE(destroy) { glDeleteVertexArrays(1, &g_device.dummy_vao); }

static GLuint compile_shader(const allocator_api* allocator, GLenum type,
                             const shader_stage_params* shader_stage_create_info)
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

MELON_GFX_CREATE_SHADER(create_shader)
{
    shader_handle shader_id = { MELON_GL_INVALID_ID };

    GLuint vertex_shader = compile_shader(&g_device.config.allocator, GL_VERTEX_SHADER, &shader_create_info->vertex_shader);
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

    shader_id = { program };

    return shader_id;
}

MELON_GFX_DELETE_SHADER(delete_shader) { glDeleteProgram((GLuint) shader.data); }

MELON_GFX_CREATE_BUFFER(create_buffer)
{
    buffer_handle buffer_id = { MELON_GL_INVALID_ID };

    GLuint buf     = 0;
    GLenum binding = GL_ARRAY_BUFFER;    // This is set to GL_ARRAY_BUFFER because at this stage the binding
                                         // doesn't matter. This is just to get the buffer bound.
    GLuint usage = gl_buffer_usage(buffer_create_info->usage);

    glGenBuffers(1, &buf);
    glBindBuffer(binding, buf);
    glBufferData(binding, buffer_create_info->size, buffer_create_info->data, usage);

    if (!buf)
    {
        MELON_LOG("Buffer creation error: could not create OpenGL buffer\n");
        return buffer_id;
    }

    buffer_id = { buf };

    glBindBuffer(binding, 0);

    return buffer_id;
}

MELON_GFX_DELETE_BUFFER(delete_buffer)
{
    GLuint handle = (GLuint) buffer.data;
    glDeleteBuffers(1, &handle);
}

MELON_GFX_CREATE_PIPELINE(create_pipeline)
{
    pipeline_gl new_pipeline    = { 0 };
    new_pipeline.shader_program = pipeline_create_info->shader_program;

    // TODO: Pipeline validation, dummy draw call
    GLuint gl_program = MELON_GL_HANDLE(new_pipeline.shader_program);

    size_t packed_stride = 0;
    for (size_t attrib_index = 0, gl_attrib_index = 0;
         attrib_index < MELON_MAX_ATTRIBUTES && gl_attrib_index < MELON_MAX_ATTRIBUTES;
         attrib_index++, gl_attrib_index++)
    {
        const vertex_attrib_params* attrib_params = pipeline_create_info->vertex_attribs + attrib_index;
        if (attrib_params->type == MELON_FORMAT_INVALID)
            continue;

        if (attrib_params->buffer_binding > MELON_MAX_BUFFER_ATTACHMENTS)
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

        packed_stride += new_attrib.size * vertex_data_type_bytes(attrib_params->type);
    }

    if (pipeline_create_info->stride == 0)
        new_pipeline.stride = packed_stride;
    else
        new_pipeline.stride = pipeline_create_info->stride;

    return { gl_pipeline_pool_push(&g_device.pipelines, new_pipeline) };
}

MELON_GFX_DELETE_PIPELINE(delete_pipeline)
{
    if (!gl_pipeline_pool_delete(&g_device.pipelines, pipeline.data))
        MELON_LOG("Pipeline deletion error: invalid ID.\n");
}

static void gl3_clear_pipeline(pipeline_handle pipeline_id)
{
    pipeline_gl* pipeline_gl = gl_pipeline_pool_get_p(&g_device.pipelines, pipeline_id.data);

    for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
    {
        vertex_attrib_gl* attrib = &pipeline_gl->attribs[attrib_index];
        glDisableVertexAttribArray(attrib->location);
    }

    glUseProgram(0);
}

static void gl3_bind_pipeline(draw_state* current_draw_state, const pipeline_handle pipeline_id)
{
    pipeline_gl* pipeline_gl     = gl_pipeline_pool_get_p(&g_device.pipelines, pipeline_id.data);
    current_draw_state->pipeline = pipeline_id;
    gl3_clear_pipeline(pipeline_id);

    MELON_ASSERT(MELON_GFX_HANDLE_IS_VALID(pipeline_gl->shader_program),
                 "Pipeline creation error: shader program ID invalid.");
    GLuint shader_program = MELON_GL_HANDLE(pipeline_gl->shader_program);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(shader_program);
}

static void gl3_bind_resources(pipeline_handle pipeline_id, draw_state* current_draw_state,
                               const draw_resources* draw_resources)
{
    pipeline_gl* pipeline_gl = gl_pipeline_pool_get_p(&g_device.pipelines, pipeline_id.data);

    for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
    {
        GLuint            current_buffer = 0;
        vertex_attrib_gl* attrib         = &pipeline_gl->attribs[attrib_index];

        buffer_handle buffer = draw_resources->buffers[attrib->buffer_binding];
        MELON_ASSERT(MELON_GFX_HANDLE_IS_VALID(buffer), "Buffer at binding %lu was invalid", attrib->buffer_binding);

        if (current_draw_state->resources.buffers[attrib->buffer_binding].data == buffer.data)
            continue;

        current_draw_state->resources.buffers[attrib->buffer_binding] = buffer;

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

    if (current_draw_state->resources.index_buffer.data != draw_resources->index_buffer.data
        && MELON_GFX_HANDLE_IS_VALID(current_draw_state->resources.index_buffer))
    {
        current_draw_state->resources.index_buffer = draw_resources->index_buffer;
        current_draw_state->resources.index_type   = draw_resources->index_type;

        GLuint index_buffer = MELON_GL_HANDLE(current_draw_state->resources.index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    }
}

static GLenum gl_draw_type(draw_type type)
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
MELON_GFX_EXECUTE_DRAW_GROUPS(execute_draw_groups)
{
    draw_state current_draw_state = { 0 };
    for (size_t i = 0; i < num_draw_groups; i++)
    {
        pipeline_handle       pipeline  = draw_groups[i].pipeline;
        const draw_resources* resources = &draw_groups[i].resources;

        MELON_ASSERT(gl_pipeline_pool_handle_is_valid(&g_device.pipelines, pipeline.data),
                     "Pipeline binding error: pipeline ID invalid.");

        gl3_bind_pipeline(&current_draw_state, pipeline);

        const draw_group* draw_group = &draw_groups[i];
        for (size_t j = 0; j < draw_group->num_draw_calls; j++)
        {
            const draw_call_params* draw_call = &(draw_group->draw_calls[j]);
            gl3_bind_resources(pipeline, &current_draw_state, resources);

            if (MELON_GFX_HANDLE_IS_VALID(resources->index_buffer))
            {
                glDrawElementsInstancedBaseVertex(gl_draw_type(draw_call->type), draw_call->num_vertices,
                                                  gl_data_format(resources->index_type), NULL, draw_call->instances,
                                                  draw_call->base_vertex);
            }
            else
            {
                glDrawArraysInstanced(gl_draw_type(draw_call->type), draw_call->base_vertex, draw_call->num_vertices,
                                      draw_call->instances);
            }
        }
    }
    gl3_clear_pipeline(current_draw_state.pipeline);
}

MELON_GFX_CREATE_COMMAND_BUFFER(create_command_buffer)
{
    commands::command_buffer new_cb;
    commands::create_command_buffer(&g_device.config.allocator, &new_cb, MELON_MEGABYTE(2));
    return { command_buffer_pool_push(&g_device.command_buffers, new_cb) };
}

MELON_GFX_DELETE_COMMAND_BUFFER(delete_command_buffer)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::destroy_command_buffer(p);
}

MELON_GFX_CB_BEGIN_RECORDING(begin_recording)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::begin_recording(p);
}

MELON_GFX_CB_END_RECORDING(end_recording)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::end_recording(p);
}

MELON_GFX_CB_BIND_VERTEX_BUFFER(bind_vertex_buffer)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::bind_vertex_buffer(p, buffer, binding);
}

MELON_GFX_CB_BIND_INDEX_BUFFER(bind_index_buffer)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::bind_index_buffer(p, buffer);
}

MELON_GFX_CB_BIND_PIPELINE(bind_pipeline)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::bind_pipeline(p, pipeline);
}

MELON_GFX_CB_DRAW(draw)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::draw(p, params);
}

MELON_GFX_CB_RESET(reset)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::reset(p);
}

void begin_consuming(command_buffer_handle cb)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::begin_consuming(p);
}

void end_consuming(command_buffer_handle cb)
{
    commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, cb.data);
    commands::end_consuming(p);
}

/**
 * TODO: Render passes, sorting commands by render pass
 */
MELON_GFX_CB_SUBMIT(submit_commands)
{
    // Translate to GL
    for (size_t i = 0; i < num_cbs; i++)
    {
        commands::command_buffer* p = command_buffer_pool_get_p(&g_device.command_buffers, command_buffers[i].data);
        commands::begin_consuming(p);

        commands::command* cmd = commands::pop_command(p);
        while (cmd)
        {
            switch (cmd->type)
            {
                case commands::BIND_VERTEX_BUFFER:
                {
                    commands::bind_vertex_buffer_data* bind_data = (commands::bind_vertex_buffer_data*) cmd->data;
                    break;
                }
                case commands::BIND_INDEX_BUFFER:
                {
                    buffer_handle* ib = (buffer_handle*) cmd->data;
                    break;
                }
                case commands::BIND_PIPELINE:
                {
                    pipeline_handle* pipeline = (pipeline_handle*) cmd->data;
                    break;
                }
                case commands::DRAW:
                {
                    draw_call_params* params = (draw_call_params*) cmd->data;
                    break;
                }
            }

            cmd = commands::pop_command(p);
        }

        commands::end_consuming(p);
    }
}

}    // namespace gfx
}    // namespace melon

#endif