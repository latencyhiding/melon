#ifndef MELON_GFX_BACKEND_H
#define MELON_GFX_BACKEND_H

#include <melon/core.h>

/**
 TODO:
 1. Command buffers
 2. Uniforms
 3. Textures
*/

namespace melon
{
namespace gfx
{

////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////

#define MELON_MAX_ATTRIBUTES 16
#define MELON_MAX_BLOCK_UNIFORMS 16
#define MELON_MAX_BUFFER_ATTACHMENTS 4
#define MELON_MAX_STAGE_UNIFORM_BLOCKS 4
#define MELON_MAX_STAGE_TEXTURE_SAMPLERS 16

////////////////////////////////////////////////////////////////////////////////
// description types
// - the following types are used to describe resources
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t handle;
extern handle    invalid_handle;
#define MELON_GFX_HANDLE(name) \
    typedef struct             \
    {                          \
        handle data;           \
    } name;
#define MELON_GFX_HANDLE_IS_VALID(handle) (handle.data != invalid_handle)

/* typesafe wrappers for ids of different kinds of resources
 */

MELON_GFX_HANDLE(buffer_handle);
MELON_GFX_HANDLE(uniform_block_handle);
MELON_GFX_HANDLE(texture_handle);
MELON_GFX_HANDLE(shader_handle);
MELON_GFX_HANDLE(pipeline_handle);
MELON_GFX_HANDLE(command_buffer_handle);

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
} vertex_data_type;

/* buffer_usage - Enum defining the buffer access patterns
 */
typedef enum
{
    MELON_STATIC_BUFFER,
    MELON_DYNAMIC_BUFFER,
    MELON_STREAM_BUFFER
} buffer_usage;

typedef enum
{
    MELON_TRIANGLES,
    MELON_TRIANGLE_STRIP,
    MELON_LINES,
    MELON_POINTS
} draw_type;

/* buffer - Struct defining parameters for buffer creation
 */
typedef struct
{
    void*        data;
    size_t       size;
    buffer_usage usage;
} buffer_params;

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
    const char*      name;
    size_t           buffer_binding;
    size_t           offset;
    vertex_data_type type;
    int              size;
    int              divisor;
} vertex_attrib_params;

typedef struct
{
    const char* name;
    const char* source;
    size_t      size;
} shader_stage_params;

/* shader - Struct defining a shader
 */
typedef struct
{
    shader_stage_params vertex_shader;
    shader_stage_params fragment_shader;
} shader_params;

typedef struct
{
    vertex_attrib_params vertex_attribs[MELON_MAX_ATTRIBUTES];
    size_t               stride;

    shader_handle shader_program;
} pipeline_params;

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
} uniform_type;

typedef struct
{
    buffer_handle buffers[MELON_MAX_BUFFER_ATTACHMENTS];

    buffer_handle    index_buffer;
    vertex_data_type index_type;
} draw_resources;

typedef struct
{
    draw_type type;
    size_t    instances;
    size_t    base_vertex;
    size_t    num_vertices;
} draw_call_params;

typedef struct
{
    pipeline_handle   pipeline;
    draw_resources    resources;
    draw_call_params* draw_calls;
    size_t            num_draw_calls;
} draw_group;

/* device_config - Contains a description/settings for a  device
 */
typedef struct
{
    size_t max_shaders;
    size_t max_buffers;
    size_t max_pipelines;
    size_t max_command_buffers;
} device_resource_count;

typedef struct
{
    device_resource_count resource_count;
    allocator_api  allocator;
} device_params;

typedef struct
{
    pipeline_handle pipeline;
    draw_resources  resources;
} draw_state;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

const device_params* default_gfx_device_params();
size_t vertex_data_type_bytes(const vertex_data_type type);

/* create_device - creates an opaque pointer to a device
 *  device_config - parameter struct containing the parameters of a device.
 *                  default parameters will be used if this is NULL.
 */

#define MELON_GFX_CREATE_DEVICE(name) void name(const melon::gfx::device_params* device_config)
void init(const melon::gfx::device_params* device_config = default_gfx_device_params());

#define MELON_GFX_DELETE_DEVICE(name) void name()
MELON_GFX_DELETE_DEVICE(destroy);

#define MELON_GFX_CREATE_SHADER(name) \
    melon::gfx::shader_handle name(const melon::gfx::shader_params* shader_create_info)
MELON_GFX_CREATE_SHADER(create_shader);

#define MELON_GFX_DELETE_SHADER(name) void name(melon::gfx::shader_handle shader)
MELON_GFX_DELETE_SHADER(delete_shader);

#define MELON_GFX_CREATE_BUFFER(name) \
    melon::gfx::buffer_handle name(const melon::gfx::buffer_params* buffer_create_info)
MELON_GFX_CREATE_BUFFER(create_buffer);

#define MELON_GFX_DELETE_BUFFER(name) void name(melon::gfx::buffer_handle buffer)
MELON_GFX_DELETE_BUFFER(delete_buffer);

#define MELON_GFX_CREATE_PIPELINE(name) \
    melon::gfx::pipeline_handle name(const melon::gfx::pipeline_params* pipeline_create_info)
MELON_GFX_CREATE_PIPELINE(create_pipeline);

#define MELON_GFX_DELETE_PIPELINE(name) void name(melon::gfx::pipeline_handle pipeline)
MELON_GFX_DELETE_PIPELINE(delete_pipeline);

#define MELON_GFX_EXECUTE_DRAW_GROUPS(name) void name(melon::gfx::draw_group* draw_groups, size_t num_draw_groups)
MELON_GFX_EXECUTE_DRAW_GROUPS(execute_draw_groups);

#define MELON_GFX_CREATE_COMMAND_BUFFER(name) melon::gfx::command_buffer_handle name()
MELON_GFX_CREATE_COMMAND_BUFFER(create_command_buffer);

#define MELON_GFX_DELETE_COMMAND_BUFFER(name) void name(melon::gfx::command_buffer_handle cb)
MELON_GFX_DELETE_COMMAND_BUFFER(delete_command_buffer);

#define MELON_GFX_CB_BEGIN_RECORDING(name) void name(melon::gfx::command_buffer_handle cb)
MELON_GFX_CB_BEGIN_RECORDING(begin_recording);

#define MELON_GFX_CB_END_RECORDING(name) void name(melon::gfx::command_buffer_handle cb)
MELON_GFX_CB_END_RECORDING(end_recording);

#define MELON_GFX_CB_BIND_VERTEX_BUFFER(name) \
    void name(melon::gfx::command_buffer_handle cb, melon::gfx::buffer_handle buffer, size_t binding)
MELON_GFX_CB_BIND_VERTEX_BUFFER(bind_vertex_buffer);

#define MELON_GFX_CB_BIND_INDEX_BUFFER(name) \
    void name(melon::gfx::command_buffer_handle cb, melon::gfx::buffer_handle buffer)
MELON_GFX_CB_BIND_INDEX_BUFFER(bind_index_buffer);

#define MELON_GFX_CB_BIND_PIPELINE(name) \
    void name(melon::gfx::command_buffer_handle cb, melon::gfx::pipeline_handle pipeline)
MELON_GFX_CB_BIND_PIPELINE(bind_pipeline);

#define MELON_GFX_CB_DRAW(name) \
    void name(melon::gfx::command_buffer_handle cb, const melon::gfx::draw_call_params* params)
MELON_GFX_CB_DRAW(draw);

#define MELON_GFX_CB_RESET(name) void name(melon::gfx::command_buffer_handle cb)
MELON_GFX_CB_RESET(reset);

#define MELON_GFX_CB_SUBMIT(name) \
    void name(melon::gfx::command_buffer_handle* command_buffers, size_t num_cbs)
MELON_GFX_CB_SUBMIT(submit_command_buffers);

}    // namespace gfx
}    // namespace melon

#endif