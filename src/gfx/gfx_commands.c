#include <melon/core/memory.h>

#include "gfx_commands.h"

void cb_create(const melon_allocator_api* alloc, cb_command_buffer* cb, size_t block_size)
{
    cb->recording = false;
    cb->consuming = false;
    mtx_init(&cb->mtx, mtx_plain);

    cb->memory = melon_create_arena(block_size, MELON_DEFAULT_ALIGN, alloc);

    cb->last = NULL;
}

void cb_destroy(cb_command_buffer* cb)
{
    mtx_destroy(&cb->mtx);

    melon_destroy_arena(&cb->memory);
}

void cb_begin_recording(cb_command_buffer* cb)
{
    // Attempt to pass submission fence, ie: do not begin recording until submission involving this command
    // buffer is completed
    mtx_lock(&cb->mtx);
    cb->recording = true;
}

void cb_end_recording(cb_command_buffer* cb)
{
    // Block any operations until submission is complete
    cb->recording = false;
    mtx_unlock(&cb->mtx);
}

void* cb_push_command(cb_command_buffer* cb, size_t size, size_t align, cb_command_type type)
{
    MELON_ASSERT(!cb->recording, "Don't push commands outside of melon_begin_recording() and melon_end_recording() calls!");

    cb_command* cmd = MELON_ARENA_PUSH_STRUCT(cb->memory, cb_command);
    cmd->type       = type;
    cmd->data       = MELON_ARENA_PUSH(cb->memory, size, align);
    cmd->next       = NULL;

    if (cb->last)
        cb->last->next = cmd;
    cb->last = cmd;

    return cmd->data;
}

void cb_reset(cb_command_buffer* cb)
{
    mtx_lock(&cb->mtx);

    // Reset command buffer
    melon_arena_reset(&cb->memory);

    mtx_unlock(&cb->mtx);
}

void cb_begin_consuming(cb_command_buffer* cb)
{
    mtx_lock(&cb->mtx);
    cb->consuming = true;
}

void cb_end_consuming(cb_command_buffer* cb)
{
    cb->consuming = false;
    mtx_unlock(&cb->mtx);
}

cb_command* cb_pop_command(cb_command_buffer* cb)
{
    MELON_ASSERT(!cb->consuming, "Don't pop commands outside of melon_begin_consuming() and melon_end_consuming() calls!");

    cb_command* result = cb->first;

    if (result)
        cb->first = result->next;

    return result;
}

void cb_cmd_bind_vertex_buffer(cb_command_buffer* cb, melon_buffer_handle buffer, size_t binding)
{
    cb_cmd_bind_vertex_buffer_data* bind_data = (cb_cmd_bind_vertex_buffer_data*) cb_push_command(
        cb, sizeof(cb_cmd_bind_vertex_buffer_data), MELON_DEFAULT_ALIGN, MELON_CMD_BIND_VERTEX_BUFFER);
    bind_data->binding = binding;
}

void cb_cmd_bind_index_buffer(cb_command_buffer* cb, melon_buffer_handle buffer)
{
    melon_buffer_handle* ib_buffer = (melon_buffer_handle*) cb_push_command(
        cb, sizeof(melon_buffer_handle), MELON_DEFAULT_ALIGN, MELON_CMD_BIND_INDEX_BUFFER);
    *ib_buffer = buffer;
}

void cb_cmd_bind_pipeline(cb_command_buffer* cb, melon_pipeline_handle pipeline)
{
    melon_pipeline_handle* pipeline_ptr = (melon_pipeline_handle*) cb_push_command(
        cb, sizeof(melon_pipeline_handle), MELON_DEFAULT_ALIGN, MELON_CMD_BIND_PIPELINE);
    *pipeline_ptr = pipeline;
}

void cb_cmd_draw(cb_command_buffer* cb, const melon_draw_call_params* params)
{
    melon_draw_call_params* dc = (melon_draw_call_params*) cb_push_command(cb, sizeof(melon_draw_call_params),
                                                                       MELON_DEFAULT_ALIGN, MELON_CMD_DRAW);
    *dc                      = *params;
}