#include "gfx_commands.h"

namespace melon
{

namespace gfx
{

namespace commands
{

void create_command_buffer(const allocator_api* alloc, command_buffer* cb, size_t block_size)
{
    cb->recording = false;
    cb->consuming = false;
    mtx_init(&cb->mtx, mtx_plain);

    cb->memory = create_arena(block_size, MELON_DEFAULT_ALIGN, alloc);

    cb->last = NULL;
}

void destroy_command_buffer(command_buffer* cb)
{
    mtx_destroy(&cb->mtx);

    destroy_arena(&cb->memory);
}

void begin_recording(command_buffer* cb)
{
    // Attempt to pass submission fence, ie: do not begin recording until submission involving this command
    // buffer is completed
    mtx_lock(&cb->mtx);
    cb->recording = true;
}

void end_recording(command_buffer* cb)
{
    // Block any operations until submission is complete
    cb->recording = false;
    mtx_unlock(&cb->mtx);
}

void* push_command(command_buffer* cb, size_t size, size_t align, command_type type)
{
    MELON_ASSERT(!cb->recording, "Don't push commands outside of begin_recording() and end_recording() calls!");

    command* cmd = MELON_ARENA_PUSH_STRUCT(cb->memory, command);
    cmd->type    = type;
    cmd->data    = MELON_ARENA_PUSH(cb->memory, size, align);
    cmd->next    = NULL;

    if (cb->last)
        cb->last->next = cmd;
    cb->last = cmd;

    return cmd->data;
}

void reset(command_buffer* cb)
{
    mtx_lock(&cb->mtx);

    // Reset command buffer
    arena_reset(&cb->memory);

    mtx_unlock(&cb->mtx);
}

void begin_consuming(command_buffer* cb)
{
    mtx_lock(&cb->mtx);
    cb->consuming = true;
}

void end_consuming(command_buffer* cb)
{
    cb->consuming = false;
    mtx_unlock(&cb->mtx);
}

command* pop_command(command_buffer* cb)
{
    MELON_ASSERT(!cb->consuming, "Don't pop commands outside of begin_consuming() and end_consuming() calls!");

    command* result = cb->first;

    if (result)
        cb->first = result->next;

    return result;
}

void bind_vertex_buffer(command_buffer* cb, buffer_handle buffer, size_t binding)
{
    bind_vertex_buffer_data* bind_data = (bind_vertex_buffer_data*) push_command(
        cb, sizeof(bind_vertex_buffer_data), MELON_DEFAULT_ALIGN, BIND_VERTEX_BUFFER);
    bind_data->binding = binding;
}

void bind_index_buffer(command_buffer* cb, buffer_handle buffer)
{
    buffer_handle* ib_buffer
        = (buffer_handle*) push_command(cb, sizeof(buffer_handle), MELON_DEFAULT_ALIGN, BIND_INDEX_BUFFER);
    *ib_buffer = buffer;
}

void bind_pipeline(command_buffer* cb, pipeline_handle pipeline)
{
    pipeline_handle* pipeline_ptr
        = (pipeline_handle*) push_command(cb, sizeof(pipeline_handle), MELON_DEFAULT_ALIGN, BIND_PIPELINE);
    *pipeline_ptr = pipeline;
}

void draw(command_buffer* cb, const draw_call_params* params)
{
    draw_call_params* dc
        = (draw_call_params*) push_command(cb, sizeof(draw_call_params), MELON_DEFAULT_ALIGN, DRAW);
    *dc = *params;
}

}
}
}