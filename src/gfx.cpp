#include <melon/error.hpp>
#include <melon/gfx.hpp>

#include <stdint.h>
#include <tinycthread.h>

namespace melon
{

namespace gfx
{

create_device_f* api_create_device;
delete_device_f* api_delete_device;

create_shader_f* create_shader;
delete_shader_f* delete_shader;

create_buffer_f* create_buffer;
delete_buffer_f* delete_buffer;

create_pipeline_f* create_pipeline;
delete_pipeline_f* delete_pipeline;

execute_draw_groups_f* execute_draw_groups;

create_command_buffer_f* create_command_buffer;
delete_command_buffer_f* delete_command_buffer;

cb_begin_recording_f* cb_begin_recording;
cb_end_recording_f* cb_end_recording;
cb_bind_vertex_buffer_f* cb_bind_vertex_buffer;
cb_bind_index_buffer_f* cb_bind_index_buffer;
cb_bind_pipeline_f* cb_bind_pipeline;
cb_draw_f* cb_draw;

cb_reset_f* cb_reset;

submit_commands_f* submit_commands;

handle invalid_handle;

typedef struct
{
    device_resource_count resource_count;
    allocator_api         allocator;
} device;

static device g_device;

static size_t vertex_data_type_bytes(vertex_data_type type)
{
    switch (type)
    {
        case MELON_FORMAT_BYTE:
        case MELON_FORMAT_UBYTE: return sizeof(uint8_t);
        case MELON_FORMAT_SHORT:
        case MELON_FORMAT_USHORT: return sizeof(uint16_t);
        case MELON_FORMAT_INT:
        case MELON_FORMAT_UINT: return sizeof(uint32_t);
        case MELON_FORMAT_HALF: return sizeof(float) / 2;
        case MELON_FORMAT_FLOAT: return sizeof(float);
        default: return 0;
    }
}

typedef struct
{
    pipeline_handle pipeline;
    draw_resources  resources;
} draw_state;

////////////////////////////////////////////////////////////////////////////////
// COMMAND BUFFER
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    buffer_handle buffer;
    size_t        binding;
} bind_vertex_buffer_data;

typedef enum
{
    BIND_VERTEX_BUFFER,
    BIND_INDEX_BUFFER,
    BIND_PIPELINE,
    DRAW
} command_type;

typedef struct command
{
    command_type type;

    void*    data;
    command* next;
} command;

typedef struct
{
    memory_arena    memory;
    draw_resources  current_resources;
    pipeline_handle current_pipeline;

    bool  consuming;
    bool  recording;
    mtx_t mtx;

    command* first;
    command* last;

    size_t num_commands;
} command_buffer;

/**
 * TODO: When recording a draw command, also record required bindings, instead of making them discrete commands in the
 * buffer
 */

static void _cb_create(device* device, command_buffer* cb, size_t block_size)
{
    cb->recording = false;
    cb->consuming = false;
    mtx_init(&cb->mtx, mtx_plain);

    cb->memory = create_arena(block_size, MELON_DEFAULT_ALIGN, &device->allocator);

    cb->last = NULL;
}

static void _cb_destroy(command_buffer* cb)
{
    mtx_destroy(&cb->mtx);

    destroy_arena(&cb->memory);
}

static void _cb_begin_recording(command_buffer* cb)
{
    // Attempt to pass submission fence, ie: do not begin recording until submission involving this command
    // buffer is completed
    mtx_lock(&cb->mtx);
    cb->recording = true;
    cb->consuming = false;
}

static void _cb_end_recording(command_buffer* cb)
{
    // Block any operations until submission is complete
    cb->recording = false;
    mtx_unlock(&cb->mtx);
}

static void* _cb_push_command(command_buffer* cb, size_t size, size_t align, command_type type)
{
    command* cmd = MELON_ARENA_PUSH_STRUCT(cb->memory, command);
    cmd->type    = type;
    cmd->data    = MELON_ARENA_PUSH(cb->memory, size, align);
    cmd->next    = NULL;

    if (cb->last)
        cb->last->next = cmd;
    cb->last = cmd;

    return cmd->data;
}

static void _cb_bind_vertex_buffer(command_buffer* cb, buffer_handle buffer, size_t binding)
{
    if (!cb->recording)
        return;

    bind_vertex_buffer_data* bind_data = (bind_vertex_buffer_data*) _cb_push_command(
        cb, sizeof(bind_vertex_buffer_data), MELON_DEFAULT_ALIGN, BIND_VERTEX_BUFFER);
    bind_data->binding = binding;
}

static void _cb_bind_index_buffer(command_buffer* cb, buffer_handle buffer)
{
    if (!cb->recording)
        return;

    buffer_handle* ib_buffer
        = (buffer_handle*) _cb_push_command(cb, sizeof(buffer_handle), MELON_DEFAULT_ALIGN, BIND_INDEX_BUFFER);
    *ib_buffer = buffer;
}

static void _cb_bind_pipeline(command_buffer* cb, pipeline_handle pipeline)
{
    if (!cb->recording)
        return;

    pipeline_handle* pipeline_ptr
        = (pipeline_handle*) _cb_push_command(cb, sizeof(pipeline_handle), MELON_DEFAULT_ALIGN, BIND_PIPELINE);
    *pipeline_ptr = pipeline;
}

static void _cb_draw(command_buffer* cb, const draw_call_params* params)
{
    if (!cb->recording)
        return;

    draw_call_params* dc
        = (draw_call_params*) _cb_push_command(cb, sizeof(draw_call_params), MELON_DEFAULT_ALIGN, DRAW);
    *dc = *params;
}

static void _cb_reset(command_buffer* cb)
{
    mtx_lock(&cb->mtx);

    // Reset command buffer
    arena_reset(&cb->memory);

    mtx_unlock(&cb->mtx);
}

static void _cb_begin_consuming(command_buffer* cb)
{
    mtx_lock(&cb->mtx);
    cb->consuming = true;
    cb->recording = false;
}

static void _cb_end_consuming(command_buffer* cb)
{
    cb->consuming = false;
    mtx_unlock(&cb->mtx);
}

static command* _cb_pop_command(command_buffer* cb)
{
    if (!cb->consuming)
        return NULL;

    command* result = cb->first;

    if (result)
        cb->first = result->next;

    return result;
}

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
MELON_POOL_VECTOR(command_buffer, command_buffer)

typedef struct
{
    gl_pipeline_pool    pipelines;
    command_buffer_pool command_buffers;
    GLuint              dummy_vao;
} device_gl;

static device_gl g_device_gl;

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

MELON_GFX_CREATE_DEVICE(create_device_gl3)
{
    create_gl_pipeline_pool(&g_device_gl.pipelines, device_config->resource_count.max_pipelines,
                            device_config->allocator);
    g_device_gl.dummy_vao = 0;

    glGenVertexArrays(1, &g_device_gl.dummy_vao);
    glBindVertexArray(g_device_gl.dummy_vao);
}

MELON_GFX_DELETE_DEVICE(delete_device_gl3) { glDeleteVertexArrays(1, &g_device_gl.dummy_vao); }

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

MELON_GFX_CREATE_SHADER(create_shader_gl3)
{
    shader_handle shader_id = { MELON_GL_INVALID_ID };

    GLuint vertex_shader = compile_shader(&g_device.allocator, GL_VERTEX_SHADER, &shader_create_info->vertex_shader);
    GLuint fragment_shader
        = compile_shader(&g_device.allocator, GL_FRAGMENT_SHADER, &shader_create_info->fragment_shader);

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

        char* error_buffer = (char*) MELON_ALLOC(g_device.allocator, sizeof(char) * len, MELON_DEFAULT_ALIGN);
        glGetProgramInfoLog(program, len, &len, error_buffer);

        MELON_LOG("Shader linking error: %s\n", error_buffer);

        MELON_FREE(g_device.allocator, error_buffer);
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

MELON_GFX_DELETE_SHADER(delete_shader_gl3) { glDeleteProgram((GLuint) shader.data); }

MELON_GFX_CREATE_BUFFER(create_buffer_gl3)
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

MELON_GFX_DELETE_BUFFER(delete_buffer_gl3)
{
    GLuint handle = (GLuint) buffer.data;
    glDeleteBuffers(1, &handle);
}

MELON_GFX_CREATE_PIPELINE(create_pipeline_gl3)
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

    return { gl_pipeline_pool_push(&g_device_gl.pipelines, new_pipeline) };
}

MELON_GFX_DELETE_PIPELINE(delete_pipeline_gl3)
{
    if (!gl_pipeline_pool_delete(&g_device_gl.pipelines, pipeline.data))
        MELON_LOG("Pipeline deletion error: invalid ID.\n");
}

static void gl3_clear_pipeline(pipeline_handle pipeline_id)
{
    pipeline_gl* pipeline_gl = gl_pipeline_pool_get_p(&g_device_gl.pipelines, pipeline_id.data);

    for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
    {
        vertex_attrib_gl* attrib = &pipeline_gl->attribs[attrib_index];
        glDisableVertexAttribArray(attrib->location);
    }

    glUseProgram(0);
}

static void gl3_bind_pipeline(draw_state* current_draw_state, const pipeline_handle pipeline_id)
{
    pipeline_gl* pipeline_gl     = gl_pipeline_pool_get_p(&g_device_gl.pipelines, pipeline_id.data);
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
    pipeline_gl* pipeline_gl = gl_pipeline_pool_get_p(&g_device_gl.pipelines, pipeline_id.data);

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
MELON_GFX_EXECUTE_DRAW_GROUPS(execute_draw_groups_gl3)
{
    draw_state current_draw_state = { 0 };
    for (size_t i = 0; i < num_draw_groups; i++)
    {
        pipeline_handle       pipeline  = draw_groups[i].pipeline;
        const draw_resources* resources = &draw_groups[i].resources;

        MELON_ASSERT(gl_pipeline_pool_index_is_valid(&g_device_gl.pipelines, pipeline.data),
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

MELON_GFX_CREATE_COMMAND_BUFFER(create_command_buffer_gl3)
{
    command_buffer new_cb;
    _cb_create(&g_device, &new_cb, MELON_MEGABYTE(2));
    return { command_buffer_pool_push(&g_device_gl.command_buffers, new_cb) };
}

MELON_GFX_DELETE_COMMAND_BUFFER(delete_command_buffer_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_destroy(cb_p);
}

MELON_GFX_CB_BEGIN_RECORDING(cb_begin_recording_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_begin_recording(cb_p);
}

MELON_GFX_CB_END_RECORDING(cb_end_recording_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_end_recording(cb_p);
}

MELON_GFX_CB_BIND_VERTEX_BUFFER(cb_bind_vertex_buffer_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_bind_vertex_buffer(cb_p, buffer, binding);
}

MELON_GFX_CB_BIND_INDEX_BUFFER(cb_bind_index_buffer_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_bind_index_buffer(cb_p, buffer);
}

MELON_GFX_CB_BIND_PIPELINE(cb_bind_pipeline_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_bind_pipeline(cb_p, pipeline);
}

MELON_GFX_CB_DRAW(cb_draw_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_draw(cb_p, params);
}

MELON_GFX_CB_RESET(cb_reset_gl3)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_reset(cb_p);
}

void cb_begin_consuming_gl3(command_buffer_handle cb)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_begin_consuming(cb_p);
}

void cb_end_consuming_gl3(command_buffer_handle cb)
{
    command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, cb.data);
    _cb_end_consuming(cb_p);
}

/**
 * TODO: Render passes, sorting commands by render pass
 */
MELON_GFX_CB_SUBMIT(submit_commands_gl3)
{
    // Translate to GL
    for (size_t i = 0; i < num_cbs; i++)
    {
        command_buffer* cb_p = command_buffer_pool_get_p(&g_device_gl.command_buffers, command_buffers[i].data);
        _cb_begin_consuming(cb_p);

        command* cmd = _cb_pop_command(cb_p);
        while (cmd)
        {
            switch (cmd->type)
            {
                case BIND_VERTEX_BUFFER:
                {
                    bind_vertex_buffer_data* bind_data = (bind_vertex_buffer_data*) cmd->data;
                    break;
                }
                case BIND_INDEX_BUFFER:
                {
                    buffer_handle* ib = (buffer_handle*) cmd->data;
                    break;
                }
                case BIND_PIPELINE:
                {
                    pipeline_handle* pipeline = (pipeline_handle*) cmd->data;
                    break;
                }
                case DRAW:
                {
                    draw_call_params* params = (draw_call_params*) cmd->data;
                    break;
                }
            }

            cmd = _cb_pop_command(cb_p);
        }

        _cb_end_consuming(cb_p);
    }
}

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

device_params default_gfx_device_params()
{
    device_params result = {};
    {
        result.resource_count = {};
        {
            result.resource_count.max_shaders         = 256;
            result.resource_count.max_buffers         = 256;
            result.resource_count.max_pipelines       = 256;
            result.resource_count.max_command_buffers = 256;
        }
        result.allocator    = default_cb_allocator();
        result.graphics_api = device_params::OPENGL3;
    };
    return result;
}

void init(device_params* device_config)
{
    if (device_config == NULL)
    {
        static device_params default_device_config = { 0 };
        default_device_config                      = default_gfx_device_params();
        device_config                              = &default_device_config;
    }

    device_config->allocator = device_config->allocator ? device_config->allocator : default_cb_allocator();

    g_device.allocator      = *(device_config->allocator);
    g_device.resource_count = device_config->resource_count;

    switch (device_config->graphics_api)
    {
        case device_params::OPENGL3:
            api_create_device = create_device_gl3;
            api_delete_device = delete_device_gl3;

            create_shader = create_shader_gl3;
            delete_shader = delete_shader_gl3;

            create_buffer = create_buffer_gl3;
            delete_buffer = delete_buffer_gl3;

            create_pipeline = create_pipeline_gl3;
            delete_pipeline = delete_pipeline_gl3;

            execute_draw_groups = execute_draw_groups_gl3;

            create_command_buffer = create_command_buffer_gl3;
            delete_command_buffer = delete_command_buffer_gl3;

            cb_begin_recording    = cb_begin_recording_gl3;
            cb_end_recording      = cb_end_recording_gl3;
            cb_bind_vertex_buffer = cb_bind_vertex_buffer_gl3;
            cb_bind_index_buffer  = cb_bind_index_buffer_gl3;
            cb_bind_pipeline      = cb_bind_pipeline_gl3;
            cb_draw               = cb_draw_gl3;

            cb_reset = cb_reset_gl3;

            submit_commands = submit_commands_gl3;

            invalid_handle = MELON_GL_INVALID_ID;
            break;
    }

    api_create_device(device_config);
}

void destroy() { api_delete_device(); }

}    // namespace gfx
}    // namespace melon