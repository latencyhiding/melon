#ifndef TZ_GFX_H
#define TZ_GFX_H

#include <stddef.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////

#define TZ_MAX_ATTRIBUTES 16
#define TZ_MAX_BLOCK_UNIFORMS 16
#define TZ_MAX_BUFFER_ATTACHMENTS 4
#define TZ_MAX_STAGE_UNIFORM_BLOCKS 4
#define TZ_MAX_STAGE_TEXTURE_SAMPLERS 16
#define TZ_MAX_SHADER_RESOURCES_PER_SET 20
#define TZ_MAX_RESOURCE_SETS_PER_PIPELINE 4

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

/* tz_pool - a pool with a stack-like behavior. Indices are allocated in a FIFO
 * order.
 */
#define TZ_POOL_INVALID_INDEX ~0
#define TZ_INVALID_ID(id) (id.index == TZ_POOL_INVALID_INDEX)
typedef uint32_t tz_pool_index;
#define TZ_ID(name) typedef struct {tz_pool_index id;} name;

typedef struct
{
  tz_pool_index* free_indices;
  size_t capacity;
  size_t num_free_indices;

  tz_cb_allocator allocator;
} tz_pool;

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_cb_allocator* allocator);
void tz_delete_pool(tz_pool* pool);
tz_pool_index tz_pool_create_id(tz_pool* pool);
void tz_pool_delete_id(tz_pool* pool, tz_pool_index index);

/* tz_*_id - typesafe wrappers for ids of different kinds of resources
 */

TZ_ID(tz_buffer);
TZ_ID(tz_vertex_format);
TZ_ID(tz_uniform_block);
TZ_ID(tz_texture);
TZ_ID(tz_shader_stage);
TZ_ID(tz_shader);
TZ_ID(tz_resource_package);
TZ_ID(tz_pipeline);

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
} tz_buffer_params;

/* tz_vertex_attrib - Struct defining vertex attributes 
 *
 * buffer_binding - binding index of the buffer that this attribute exists in
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
  int buffer_binding;
  size_t offset;
  tz_vertex_data_type type;
  int size;
  int divisor;
} tz_vertex_attrib_params;

/* tz_vertex_format - Struct defining vertex formats
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

  tz_vertex_attrib_params attribs[TZ_MAX_ATTRIBUTES];
} tz_vertex_format_params;

/* tz_shader_stage - Struct defining a shader stage
 * NOTE: for simplicity's sake, shaders are considered a single unit when resources
 * are concerned. Uniforms are bound for every stage.
 */
typedef struct
{
  const char* source;
  size_t size;
} tz_shader_stage_params;

/* tz_shader - Struct defining a shader
*/
typedef struct
{
  tz_shader_stage vertex_shader;
  tz_shader_stage fragment_shader;
} tz_shader_params;

typedef struct
{
  tz_vertex_format vertex_format;
  tz_shader shader_program;
} tz_pipeline_params;

/* tz_gfx_device_config - Contains a description/settings for a gfx device
 */
typedef struct
{
  size_t max_shader_stages;
  size_t max_shaders;
  size_t max_buffers;
  size_t max_vertex_formats;
  size_t max_pipelines;

  tz_cb_allocator allocator;
} tz_gfx_device_params;

////////////////////////////////////////////////////////////////////////////////
// Operation Types
// - Types used to operate the backend
////////////////////////////////////////////////////////////////////////////////

/* tz_gfx_device - opaque type for the backend data and the allocator
 */
typedef struct tz_gfx_device tz_gfx_device;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

#define TZ_ALLOC(allocator, size, align) (allocator.alloc(allocator.user_data, size, align))
#define TZ_REALLOC(allocator, ptr, size, align) (allocator.realloc(allocator.user_data, ptr, size, align))
#define TZ_FREE(allocator, ptr) (allocator.dealloc(allocator.user_data, ptr))

// Returns the default allocator for our backend
const tz_cb_allocator* tz_default_cb_allocator();

/// TODO
#define TZ_GFX_API_FUNC
///

tz_gfx_device* tz_create_device(const tz_gfx_device_params* device_config);
void tz_delete_device(tz_gfx_device* device);

tz_shader_stage tz_create_shader_stage(tz_gfx_device* device, const tz_shader_stage_params* shader_stage_create_info);
tz_shader tz_create_shader(tz_gfx_device* device, const tz_shader_params* shader_create_info);
tz_buffer tz_create_buffer(tz_gfx_device* device, const tz_buffer_params* buffer_create_info);
tz_vertex_format tz_create_vertex_format(tz_gfx_device* device, const tz_vertex_format_params* vertex_format_info);

#endif 
