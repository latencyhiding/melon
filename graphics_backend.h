#ifndef TZ_GFX_H
#define TZ_GFX_H

#include <stddef.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////

#define TZ_MAX_ATTRIBUTES 16
#define TZ_MAX_BUFFER_ATTACHMENTS 4
#define TZ_MAX_STAGE_UNIFORM_BLOCKS 4
#define TZ_MAX_BLOCK_UNIFORMS 16
#define TZ_MAX_STAGE_TEXTURE_SAMPLERS 16

////////////////////////////////////////////////////////////////////////////////
// Description Types 
// - The following types are used to describe resources
////////////////////////////////////////////////////////////////////////////////

// Functors for allocation callbacks
typedef struct
{
  void* (*alloc) (void* user_data, size_t size, size_t align);
  void* (*realloc) (void* user_data, void* ptr, size_t size, size_t align);
  void (*dealloc) (void* user_data, void* ptr);

  void* user_data;
} tz_cb_allocator;

/* tz_*_id - typesafe wrappers for ids of different kinds of resources
 */
typedef uint32_t tz_id;
#define TZ_ID(name) typedef struct {tz_id id;} name;

TZ_ID(tz_buffer_id);
TZ_ID(tz_buffer_descriptor_id);
TZ_ID(tz_uniform_block_id);
TZ_ID(tz_texture_id);
TZ_ID(tz_shader_stage_id);
TZ_ID(tz_shader_id);
TZ_ID(tz_pipeline_id);

// Basic vertex data types
typedef enum
{
  TZ_FORMAT_INVALID,
  TZ_FORMAT_BYTE,
  TZ_FORMAT_UBYTE,
  TZ_FORMAT_SHORT,
  TZ_FORMAT_USHORT,
  TZ_FORMAT_INT,
  TZ_FORMAT_UINT,
  TZ_FORMAT_HALF,
  TZ_FORMAT_FLOAT,
  TZ_FORMAT_DOUBLE,
  TZ_FORMAT_FIXED
} tz_vertex_data_type;

/* tz_buffer_type - Enum defining types of buffers
*/

typedef enum
{
  TZ_ARRAY_BUFFER,
  TZ_ELEMENT_BUFFER
} tz_buffer_type;

/* tz_buffer_usage - Enum defining the buffer access patterns
*/
typedef enum
{
  TZ_STATIC_BUFFER,
  TZ_DYNAMIC_BUFFER,
  TZ_STREAM_BUFFER
} tz_buffer_usage;

/* tz_buffer - Struct defining parameters for buffer creation
*/
typedef struct
{
  void* data;
  size_t size;
  tz_buffer_usage usage;
} tz_buffer;

/* tz_vertex_attrib - Struct defining vertex attributes 
 *
 * name - name of the attribute. can be null
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
  const char* name;
  size_t offset;
  tz_vertex_data_type type;
  int size;
  int divisor;
} tz_vertex_attrib;

/* tz_buffer_descriptor - Struct defining vertex formats
 *
 * type - type of the buffer
 * stride - number of bytes between attributes of one vertex to the next, aka the
 *          size of the vertex
 * num_attribs - number of attributes in a vertex, maximum TZ_MAX_ATTRIBUTES
 * attribs - array containing vertex attributes
 */
typedef struct
{
  tz_buffer_type type;

  size_t stride;
  size_t num_attribs;

  tz_vertex_attrib attribs[TZ_MAX_ATTRIBUTES];
} tz_buffer_descriptor;

typedef enum
{
  TZ_TEXTURE_1D,
  TZ_TEXTURE_2D,
  TZ_TEXTURE_3D,
  TZ_TEXTURE_CUBE
} tz_texture_type;

// TODO: Textures

/* tz_texture_sampler - Struct defining a texture sampler
*/
typedef struct
{
  const char* name;
  tz_texture_type sampler_type; 
} tz_texture_sampler;

/* tz_uniform_type - Enum describing types of uniforms
*/
typedef enum 
{
  TZ_UNIFORM_FLOAT,
  TZ_UNIFORM_FLOAT2,
  TZ_UNIFORM_FLOAT3,
  TZ_UNIFORM_FLOAT4,
  TZ_UNIFORM_INT,
  TZ_UNIFORM_INT2,
  TZ_UNIFORM_INT3,
  TZ_UNIFORM_INT4,
  TZ_UNIFORM_UINT,
  TZ_UNIFORM_UINT2,
  TZ_UNIFORM_UINT3,
  TZ_UNIFORM_UINT4,
  TZ_UNIFORM_MAT2x2,
  TZ_UNIFORM_MAT3x3,
  TZ_UNIFORM_MAT4x4
} tz_uniform_type;

/* tz_uniform - Struct defining a uniform 
 * name - the name of the uniform variable in the block
 * type - the datatype of the uniform
 * count - number of data in an array
 * offset - offset of the uniform in the data block provided by the user; NOT the offset
 *          in the shader. actual packing rules in the shader are backend specific, for
 *          example OpenGL uses its std140 packing, DirectX packing is different.
 */
typedef struct 
{
  const char* name;
  tz_uniform_type type;
  size_t count;
  size_t offset;
} tz_uniform;

/* tz_uniform_block - Struct defining a uniform block
*/
typedef struct
{
  tz_uniform uniforms[TZ_MAX_BLOCK_UNIFORMS];
  int num_uniforms;
} tz_uniform_block;

/* tz_shader_stage - Struct defining a shader stage
 * NOTE: Shaders are separated into stages with separate uniform blocks in preparation
 * for a Dx11 implementation
 */
typedef struct
{
  const char* source;
  size_t size;

  tz_uniform_block_id uniform_blocks[TZ_MAX_STAGE_UNIFORM_BLOCKS];
  int num_uniform_blocks;

  tz_texture_sampler texture_samplers[TZ_MAX_STAGE_TEXTURE_SAMPLERS];
  int num_texture_samplers;
} tz_shader_stage;

/* tz_shader - Struct defining a shader
*/
typedef struct
{
  tz_shader_stage_id vertex_shader;
  tz_shader_stage_id fragment_shader;
} tz_shader;

/* tz_gfx_device_config - Contains a description/settings for a gfx device
 */
typedef struct
{
  size_t max_shader_stages;
  size_t max_shaders;
  size_t max_buffers;
  size_t max_buffer_descriptors;
  size_t max_uniform_blocks;

  tz_cb_allocator allocator;
} tz_gfx_device_config;


typedef struct
{
  tz_buffer_descriptor_id buffer_descriptors[TZ_MAX_BUFFER_ATTACHMENTS];
} tz_pipeline;

////////////////////////////////////////////////////////////////////////////////
// Operation Types
// - Types used to operate the backend
////////////////////////////////////////////////////////////////////////////////

/* tz_gfx_device - Contains a pointer to the backend data and the allocator
 */
typedef struct
{
  void* data;
  tz_cb_allocator allocator;
} tz_gfx_device;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

tz_gfx_device* tz_create_device(const tz_gfx_device_config* device_config);

tz_shader_stage_id tz_create_shader_stage(tz_gfx_device* device, const tz_shader_stage* shader_stage_create_info);
tz_shader_id tz_create_shader(tz_gfx_device* device, const tz_shader* shader_create_info);
tz_buffer_id tz_create_buffer(tz_gfx_device* device, const tz_buffer* buffer_create_info);
tz_buffer_descriptor_id tz_create_buffer_descriptor(tz_gfx_device* device, const tz_buffer_descriptor* buffer_descriptor_info);
tz_uniform_block_id tz_create_uniform_block(tz_gfx_device* device, const tz_uniform_block* uniform_block_create_info);

#endif TZ_GFX_H
