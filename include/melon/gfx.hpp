#ifndef MELON_GFX_H
#define MELON_GFX_H

#include <stddef.h>
#include <stdint.h>

#include <melon/core.hpp>

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
extern handle invalid_handle;
#define MELON_GFX_HANDLE(name) \
    typedef struct             \
    {                          \
        handle data;           \
    } name;
#define MELON_GFX_HANDLE_IS_VALID(handle) (handle.data != invalid_handle)

/* typesafe wrappers for ids of different kinds of resources
 */

MELON_GFX_HANDLE(buffer);
MELON_GFX_HANDLE(uniform_block);
MELON_GFX_HANDLE(texture);
MELON_GFX_HANDLE(shader);
MELON_GFX_HANDLE(pipeline);
MELON_GFX_HANDLE(command_buffer);

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
    void *data;
    size_t size;
    buffer_usage usage;
} buffer_params;
static inline buffer_params gen_buffer_params() { return (buffer_params){0}; }

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
    const char *name;
    size_t buffer_binding;
    size_t offset;
    vertex_data_type type;
    int size;
    int divisor;
} vertex_attrib_params;
static inline vertex_attrib_params gen_vertex_attrib_params() { return (vertex_attrib_params){0}; }

typedef struct
{
    const char *name;
    const char *source;
    size_t size;
} shader_stage_params;
static inline shader_stage_params gen_shader_stage_params() { return (shader_stage_params){0}; }

/* shader - Struct defining a shader
*/
typedef struct
{
    shader_stage_params vertex_shader;
    shader_stage_params fragment_shader;
} shader_params;
static inline shader_params gen_shader_params() { return (shader_params){0}; }

typedef struct
{
    vertex_attrib_params vertex_attribs[MELON_MAX_ATTRIBUTES];
    size_t stride;

    shader shader_program;
} pipeline_params;
static inline pipeline_params gen_pipeline_params() { return (pipeline_params){0}; }

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
    buffer buffers[MELON_MAX_BUFFER_ATTACHMENTS];

    buffer index_buffer;
    vertex_data_type index_type;
} draw_resources;
static inline draw_resources gen_draw_resources() { return (draw_resources){0}; }

typedef struct
{
    draw_type draw_type;
    size_t instances;
    size_t base_vertex;
    size_t num_vertices;
} draw_call_params;

typedef struct
{
    pipeline pipeline;
    draw_resources resources;
    draw_call_params *draw_calls;
    size_t num_draw_calls;
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
    const allocator *allocator;

    enum
    {
        OPENGL3
    } graphics_api;
} device_params;

////////////////////////////////////////////////////////////////////////////////
// Operation Types
// - Types used to operate the backend
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

device_params default_device_params();
/* create_device - creates an opaque pointer to a device
*  device_config - parameter struct containing the parameters of a device.
*                  default parameters will be used if this is NULL.
*/

#define MELON_GFX_CREATE_DEVICE(name) void name(device_params *device_config)
typedef MELON_GFX_CREATE_DEVICE(create_device_f);
void init(device_params *device_config);

#define MELON_GFX_DELETE_DEVICE(name) void name()
typedef MELON_GFX_DELETE_DEVICE(delete_device_f);
void destroy();

#define MELON_GFX_CREATE_SHADER(name) shader name(const shader_params *shader_create_info)
typedef MELON_GFX_CREATE_SHADER(create_shader_f);
extern create_shader_f *create_shader;

#define MELON_GFX_DELETE_SHADER(name) void name(shader shader)
typedef MELON_GFX_DELETE_SHADER(delete_shader_f);
extern delete_shader_f *delete_shader;

#define MELON_GFX_CREATE_BUFFER(name) buffer name(const buffer_params *buffer_create_info)
typedef MELON_GFX_CREATE_BUFFER(create_buffer_f);
extern create_buffer_f *create_buffer;

#define MELON_GFX_DELETE_BUFFER(name) void name(buffer buffer)
typedef MELON_GFX_DELETE_BUFFER(delete_buffer_f);
extern delete_buffer_f *delete_buffer;

#define MELON_GFX_CREATE_PIPELINE(name) pipeline name(const pipeline_params *pipeline_create_info)
typedef MELON_GFX_CREATE_PIPELINE(create_pipeline_f);
extern create_pipeline_f *create_pipeline;

#define MELON_GFX_DELETE_PIPELINE(name) void name(pipeline pipeline)
typedef MELON_GFX_DELETE_PIPELINE(delete_pipeline_f);
extern delete_pipeline_f *delete_pipeline;

#define MELON_GFX_EXECUTE_DRAW_GROUPS(name) void name(draw_group *draw_groups, size_t num_draw_groups)
typedef MELON_GFX_EXECUTE_DRAW_GROUPS(execute_draw_groups_f);
extern execute_draw_groups_f *execute_draw_groups;

} // namespace gfx
} // namespace melon

#endif
