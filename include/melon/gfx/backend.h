#ifndef MELON_GFX_BACKEND_H
#define MELON_GFX_BACKEND_H

#include <melon/core.h>

/**
 TODO:
 1. Command buffers
 2. Uniforms
 3. Textures
*/

////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////

#define MELON_GFX_MAX_ATTRIBUTES 16
#define MELON_GFX_MAX_BLOCK_UNIFORMS 16
#define MELON_GFX_MAX_BUFFER_ATTACHMENTS 4
#define MELON_GFX_MAX_STAGE_UNIFORM_BLOCKS 4
#define MELON_GFX_MAX_STAGE_TEXTURE_SAMPLERS 16

////////////////////////////////////////////////////////////////////////////////
// description types
// - the following types are used to describe resources
////////////////////////////////////////////////////////////////////////////////

typedef uintptr_t     melon_gfx_handle;
extern melon_gfx_handle melon_gfx_invalid_handle;
#define MELON_GFX_HANDLE(name) \
    typedef struct             \
    {                          \
        melon_gfx_handle data;   \
    } name;
#define MELON_GFX_HANDLE_IS_VALID(handle) (handle.data != melon_gfx_invalid_handle)

/* typesafe wrappers for ids of different kinds of resources
 */

MELON_GFX_HANDLE(melon_buffer_handle);
MELON_GFX_HANDLE(melon_uniform_block_handle);
MELON_GFX_HANDLE(melon_texture_handle);
MELON_GFX_HANDLE(melon_shader_handle);
MELON_GFX_HANDLE(melon_pipeline_handle);
MELON_GFX_HANDLE(melon_command_buffer_handle);

#define MELON_GFX_GEN_PARAMS(type) ((type){ 0 })

// Basic vertex data types
typedef enum
{
    MELON_FORMAT_INVALID,
    MELON_FORMAT_BYTE,
    MELON_FORMAT_UBYTE,
    MELON_FORMAT_SHORT,
    MELON_FORMAT_USHORT,
    MELON_FORMAT_INT,
    MELON_FORMAT_UINT,
    MELON_FORMAT_HALF,
    MELON_FORMAT_FLOAT,
} melon_vertex_data_type;

/* melon_buffer_usage - Enum defining the buffer access patterns
 */
typedef enum
{
    MELON_STATIC_BUFFER,
    MELON_DYNAMIC_BUFFER,
    MELON_STREAM_BUFFER
} melon_buffer_usage;

typedef enum
{
    MELON_TRIANGLES,
    MELON_TRIANGLE_STRIP,
    MELON_LINES,
    MELON_POINTS
} melon_draw_type;

/* buffer - Struct defining parameters for buffer creation
 */
typedef struct
{
    void*            data;
    size_t           size;
    melon_buffer_usage usage;
} melon_buffer_params;

/* vertex_attrib - Struct defining vertex attributes
 *
 * name - name of the attribute binding in the shader
 * offset - number of bytes from the start of a vertex that an attribute begins
 * type - the data format of the attribute
 * size - number of data components
 * divisor - the rate at which vertex attributes advance during instanced rendering.
 *           a divisor of 0 advances attributes per vertex, a divisor of 1 advances
 *           attributes per instance, a divisor of 2 advances attributes per 2
 *           instances, and so on.
 */
typedef struct
{
    const char*          name;
    size_t               buffer_binding;
    size_t               offset;
    melon_vertex_data_type type;
    int                  size;
    int                  divisor;
} melon_vertex_attrib_params;

typedef struct
{
    const char* name;
    const char* source;
    size_t      size;
} melon_shader_stage_params;

/* shader - Struct defining a shader
 */
typedef struct
{
    melon_shader_stage_params vertex_shader;
    melon_shader_stage_params fragment_shader;
} melon_shader_params;

typedef struct
{
    melon_vertex_attrib_params vertex_attribs[MELON_GFX_MAX_ATTRIBUTES];
    size_t                   stride;

    melon_shader_handle shader_program;
} melon_pipeline_params;

typedef enum
{
    MELON_UNIFORM_INVALID,
    MELON_UNIFORM_FLOAT1,
    MELON_UNIFORM_FLOAT2,
    MELON_UNIFORM_FLOAT3,
    MELON_UNIFORM_FLOAT4,
    MELON_UNIFORM_INT1,
    MELON_UNIFORM_INT2,
    MELON_UNIFORM_INT3,
    MELON_UNIFORM_INT4,
    MELON_UNIFORM_UINT1,
    MELON_UNIFORM_UINT2,
    MELON_UNIFORM_UINT3,
    MELON_UNIFORM_UINT4,
    MELON_UNIFORM_MATRIX2,
    MELON_UNIFORM_MATRIX3,
    MELON_UNIFORM_MATRIX4
} melon_uniform_type;

typedef struct
{
    melon_buffer_handle buffers[MELON_GFX_MAX_BUFFER_ATTACHMENTS];

    melon_buffer_handle    index_buffer;
    melon_vertex_data_type index_type;
} melon_draw_resources;

typedef struct
{
    melon_draw_type type;
    size_t        instances;
    size_t        base_vertex;
    size_t        num_vertices;
} melon_draw_call_params;

typedef struct
{
    melon_pipeline_handle   pipeline;
    melon_draw_resources    resources;
    melon_draw_call_params* draw_calls;
    size_t                num_draw_calls;
} melon_draw_group;

/* device_config - Contains a description/settings for a  device
 */
typedef struct
{
    size_t max_shaders;
    size_t max_buffers;
    size_t max_pipelines;
    size_t max_command_buffers;
} melon_device_resource_count;

typedef struct
{
    melon_device_resource_count resource_count;
    melon_allocator_api         allocator;
} melon_device_params;

typedef struct
{
    melon_pipeline_handle pipeline;
    melon_draw_resources  resources;
} melon_draw_state;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

const melon_device_params* melon_default_device_params();
size_t                   melon_vertex_data_type_bytes(const melon_vertex_data_type type);

/* create_device - creates an opaque pointer to a device
 *  device_config - parameter struct containing the parameters of a device.
 *                  default parameters will be used if this is NULL.
 */

#define MELON_GFX_CREATE_DEVICE(name) bool name(const melon_device_params* device_config)
MELON_GFX_CREATE_DEVICE(melon_gfx_backend_init);

#define MELON_GFX_DELETE_DEVICE(name) void name()
MELON_GFX_DELETE_DEVICE(melon_gfx_backend_destroy);

#define MELON_GFX_CREATE_SHADER(name) melon_shader_handle name(const melon_shader_params* shader_create_info)
MELON_GFX_CREATE_SHADER(melon_create_shader);

#define MELON_GFX_DELETE_SHADER(name) void name(melon_shader_handle shader)
MELON_GFX_DELETE_SHADER(melon_delete_shader);

#define MELON_GFX_CREATE_BUFFER(name) melon_buffer_handle name(const melon_buffer_params* buffer_create_info)
MELON_GFX_CREATE_BUFFER(melon_create_buffer);

#define MELON_GFX_DELETE_BUFFER(name) void name(melon_buffer_handle buffer)
MELON_GFX_DELETE_BUFFER(melon_delete_buffer);

#define MELON_GFX_CREATE_PIPELINE(name) melon_pipeline_handle name(const melon_pipeline_params* pipeline_create_info)
MELON_GFX_CREATE_PIPELINE(melon_create_pipeline);

#define MELON_GFX_DELETE_PIPELINE(name) void name(melon_pipeline_handle pipeline)
MELON_GFX_DELETE_PIPELINE(melon_delete_pipeline);

#define MELON_GFX_EXECUTE_DRAW_GROUPS(name) void name(melon_draw_group* melon_draw_groups, size_t num_melon_draw_groups)
MELON_GFX_EXECUTE_DRAW_GROUPS(melon_execute_draw_groups);

#define MELON_GFX_CREATE_COMMAND_BUFFER(name) melon_command_buffer_handle name()
MELON_GFX_CREATE_COMMAND_BUFFER(melon_create_command_buffer);

#define MELON_GFX_DELETE_COMMAND_BUFFER(name) void name(melon_command_buffer_handle cb)
MELON_GFX_DELETE_COMMAND_BUFFER(melon_delete_command_buffer);

#define MELON_GFX_CB_BEGIN_RECORDING(name) void name(melon_command_buffer_handle cb)
MELON_GFX_CB_BEGIN_RECORDING(melon_begin_recording);

#define MELON_GFX_CB_END_RECORDING(name) void name(melon_command_buffer_handle cb)
MELON_GFX_CB_END_RECORDING(melon_end_recording);

#define MELON_GFX_CB_BIND_VERTEX_BUFFER(name) \
    void name(melon_command_buffer_handle cb, melon_buffer_handle buffer, size_t binding)
MELON_GFX_CB_BIND_VERTEX_BUFFER(melon_cmd_bind_vertex_buffer);

#define MELON_GFX_CB_BIND_INDEX_BUFFER(name) void name(melon_command_buffer_handle cb, melon_buffer_handle buffer)
MELON_GFX_CB_BIND_INDEX_BUFFER(melon_cmd_bind_index_buffer);

#define MELON_GFX_CB_BIND_PIPELINE(name) void name(melon_command_buffer_handle cb, melon_pipeline_handle pipeline)
MELON_GFX_CB_BIND_PIPELINE(melon_cmd_bind_pipeline);

#define MELON_GFX_CB_DRAW(name) void name(melon_command_buffer_handle cb, const melon_draw_call_params* params)
MELON_GFX_CB_DRAW(melon_cmd_draw);

#define MELON_GFX_CB_RESET(name) void name(melon_command_buffer_handle cb)
MELON_GFX_CB_RESET(melon_reset);

#define MELON_GFX_CB_SUBMIT(name) void name(melon_command_buffer_handle* command_buffers, size_t num_cbs)
MELON_GFX_CB_SUBMIT(melon_submit_command_buffers);

#endif