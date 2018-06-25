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
extern handle    invalid_handle;
#define MELON_GFX_HANDLE(name) \
    typedef struct             \
    {                          \
        handle data;           \
    } name;
#define MELON_GFX_HANDLE_IS_VALID(handle) (handle.data != invalid_handle)

/* typesafe wrappers for ids of different kinds of resources
 */

MELON_GFX_HANDLE(BufferHandle);
MELON_GFX_HANDLE(UniformBlockHandle);
MELON_GFX_HANDLE(TextureHandle);
MELON_GFX_HANDLE(ShaderHandle);
MELON_GFX_HANDLE(PipelineHandle);
MELON_GFX_HANDLE(CommandBufferHandle);

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
} VertexDataType;

/* BufferUsage - Enum defining the buffer access patterns
 */
typedef enum
{
    MELON_STATIC_BUFFER,
    MELON_DYNAMIC_BUFFER,
    MELON_STREAM_BUFFER
} BufferUsage;

typedef enum
{
    MELON_TRIANGLES,
    MELON_TRIANGLE_STRIP,
    MELON_LINES,
    MELON_POINTS
} DrawType;

#define MELON_GFX_PARAM_INIT(name) ((name){ 0 })

/* buffer - Struct defining parameters for buffer creation
 */
typedef struct
{
    void*       data;
    size_t      size;
    BufferUsage usage;
} BufferParams;

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
    const char*    name;
    size_t         buffer_binding;
    size_t         offset;
    VertexDataType type;
    int            size;
    int            divisor;
} VertexAttribParams;

typedef struct
{
    const char* name;
    const char* source;
    size_t      size;
} ShaderStageParams;

/* shader - Struct defining a shader
 */
typedef struct
{
    ShaderStageParams vertex_shader;
    ShaderStageParams fragment_shader;
} ShaderParams;

typedef struct
{
    VertexAttribParams vertex_attribs[MELON_MAX_ATTRIBUTES];
    size_t             stride;

    ShaderHandle shader_program;
} PipelineParams;

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
} UniformType;

typedef struct
{
    BufferHandle buffers[MELON_MAX_BUFFER_ATTACHMENTS];

    BufferHandle   index_buffer;
    VertexDataType index_type;
} DrawResources;

typedef struct
{
    DrawType type;
    size_t   instances;
    size_t   base_vertex;
    size_t   num_vertices;
} DrawCallParams;

typedef struct
{
    PipelineHandle  pipeline;
    DrawResources   resources;
    DrawCallParams* draw_calls;
    size_t          num_draw_calls;
} draw_group;

/* device_config - Contains a description/settings for a  device
 */
typedef struct
{
    size_t max_shaders;
    size_t max_buffers;
    size_t max_pipelines;
    size_t max_command_buffers;
} DeviceResourceCount;

typedef struct
{
    DeviceResourceCount resource_count;
    const AllocatorApi* allocator;

    enum
    {
        OPENGL3
    } graphics_api;
} DeviceParams;

////////////////////////////////////////////////////////////////////////////////
// Operation Types
// - Types used to operate the backend
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

DeviceParams default_device_params();
/* create_device - creates an opaque pointer to a device
 *  device_config - parameter struct containing the parameters of a device.
 *                  default parameters will be used if this is NULL.
 */

#define MELON_GFX_CREATE_DEVICE(name) void name(melon::gfx::DeviceParams* device_config)
typedef MELON_GFX_CREATE_DEVICE(CreateDeviceFunc);
void init(DeviceParams* device_config);

#define MELON_GFX_DELETE_DEVICE(name) void name()
typedef MELON_GFX_DELETE_DEVICE(DeleteDeviceFunc);
void destroy();

#define MELON_GFX_CREATE_SHADER(name) melon::gfx::ShaderHandle name(const melon::gfx::ShaderParams* shader_create_info)
typedef MELON_GFX_CREATE_SHADER(CreateShaderFunc);
extern CreateShaderFunc* create_shader;

#define MELON_GFX_DELETE_SHADER(name) void name(melon::gfx::ShaderHandle shader)
typedef MELON_GFX_DELETE_SHADER(DeleteShaderFunc);
extern DeleteShaderFunc* delete_shader;

#define MELON_GFX_CREATE_BUFFER(name) melon::gfx::BufferHandle name(const melon::gfx::BufferParams* buffer_create_info)
typedef MELON_GFX_CREATE_BUFFER(CreateBufferFunc);
extern CreateBufferFunc* create_buffer;

#define MELON_GFX_DELETE_BUFFER(name) void name(melon::gfx::BufferHandle buffer)
typedef MELON_GFX_DELETE_BUFFER(DeleteBufferFunc);
extern DeleteBufferFunc* delete_buffer;

#define MELON_GFX_CREATE_PIPELINE(name) \
    melon::gfx::PipelineHandle name(const melon::gfx::PipelineParams* pipeline_create_info)
typedef MELON_GFX_CREATE_PIPELINE(CreatePipelineFunc);
extern CreatePipelineFunc* create_pipeline;

#define MELON_GFX_DELETE_PIPELINE(name) void name(melon::gfx::PipelineHandle pipeline)
typedef MELON_GFX_DELETE_PIPELINE(DeletePipelineFunc);
extern DeletePipelineFunc* delete_pipeline;

#define MELON_GFX_EXECUTE_DRAW_GROUPS(name) void name(melon::gfx::draw_group* draw_groups, size_t num_draw_groups)
typedef MELON_GFX_EXECUTE_DRAW_GROUPS(ExecuteDrawGroupsFunc);
extern ExecuteDrawGroupsFunc* execute_draw_groups;

}    // namespace gfx
}    // namespace melon

#endif
