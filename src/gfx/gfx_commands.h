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

namespace melon
{
namespace gfx
{
namespace commands
{

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

void create_command_buffer(const allocator_api* alloc, command_buffer* cb, size_t block_size);
void destroy_command_buffer(command_buffer* cb);
void begin_recording(command_buffer* cb);
void end_recording(command_buffer* cb);
void* push_command(command_buffer* cb, size_t size, size_t align, command_type type);
void reset(command_buffer* cb);
void begin_consuming(command_buffer* cb);
void end_consuming(command_buffer* cb);
command* pop_command(command_buffer* cb);
void bind_vertex_buffer(command_buffer* cb, buffer_handle buffer, size_t binding);
void bind_index_buffer(command_buffer* cb, buffer_handle buffer);
void bind_pipeline(command_buffer* cb, pipeline_handle pipeline);
void draw(command_buffer* cb, const draw_call_params* params);

}
}
}
#endif