#ifndef MELON_GFX_COMMANDS_H
#define MELON_GFX_COMMANDS_H

#include <melon/gfx.h>

////////////////////////////////////////////////////////////////////////////////
// COMMAND BUFFER
// - NOT completely thread safe. Recording is intended to be done on one thread
//   at a time
// - Recording is blocked by consuming and vice versa, meaning you can call
//   submit on a consumer thread and begin recording on a recording thread, it's
//   just that they will not happen at the same time.
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    melon_buffer_handle buffer;
    size_t            binding;
} cb_cmd_bind_vertex_buffer_data;

typedef enum
{
    MELON_CMD_BIND_VERTEX_BUFFER,
    MELON_CMD_BIND_INDEX_BUFFER,
    MELON_CMD_BIND_PIPELINE,
    MELON_CMD_DRAW
} cb_command_type;

typedef struct cb_command
{
    cb_command_type type;

    void*              data;
    struct cb_command* next;
} cb_command;

typedef struct
{
    melon_memory_arena    memory;
    melon_draw_resources  current_resources;
    melon_pipeline_handle current_pipeline;

    bool  consuming;
    bool  recording;
    mtx_t mtx;

    cb_command* first;
    cb_command* last;

    size_t num_commands;
} cb_command_buffer;

/**
 * TODO: When recording a draw command, also record required bindings, instead of making them discrete commands in the
 * buffer
 */

void        cb_create(const melon_allocator_api* alloc, cb_command_buffer* cb, size_t block_size);
void        cb_destroy(cb_command_buffer* cb);
void        cb_begin_recording(cb_command_buffer* cb);
void        cb_end_recording(cb_command_buffer* cb);
void*       cb_push_command(cb_command_buffer* cb, size_t size, size_t align, cb_command_type type);
void        cb_reset(cb_command_buffer* cb);
void        cb_begin_consuming(cb_command_buffer* cb);
void        cb_end_consuming(cb_command_buffer* cb);
cb_command* cb_pop_command(cb_command_buffer* cb);

void cb_cmd_bind_vertex_buffer(cb_command_buffer* cb, melon_buffer_handle buffer, size_t binding);
void cb_cmd_bind_index_buffer(cb_command_buffer* cb, melon_buffer_handle buffer);
void cb_cmd_bind_pipeline(cb_command_buffer* cb, melon_pipeline_handle pipeline);
void cb_cmd_draw(cb_command_buffer* cb, const melon_draw_call_params* params);

#endif