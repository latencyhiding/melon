#ifndef TZ_GFX_H
#define TZ_GFX_H

#include <stddef.h>
#include <stdint.h>

#include "tz_core.h"

/**
 TODO:
 1. Uniforms
 2. Textures
*/

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
#define TZ_MAX_ATTRIB_NAME_SIZE 256

////////////////////////////////////////////////////////////////////////////////
// description types 
// - the following types are used to describe resources
////////////////////////////////////////////////////////////////////////////////

/* tz_*_id - typesafe wrappers for ids of different kinds of resources
 */

TZ_ID(tz_buffer);
TZ_ID(tz_uniform_block);
TZ_ID(tz_texture);
TZ_ID(tz_shader);
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
} tz_buffer_binding;

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
inline static tz_buffer_params tz_gen_buffer_params() { return (tz_buffer_params) { 0, 0, TZ_STATIC_BUFFER }; }

/* tz_vertex_attrib - Struct defining vertex attributes 
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
  const char* name;
  size_t offset;
  tz_vertex_data_type type;
  int size;
  int divisor;
} tz_vertex_attrib_params;
inline static tz_vertex_attrib_params tz_gen_vertex_attrib_params() { return (tz_vertex_attrib_params) { 0 }; }

/* tz_buffer_format - Struct defining the format of a buffer 
 *
 * stride - number of bytes between attributes of one vertex to the next, aka the
 *          size of the vertex. This is optional, fill in only if you have a custom stride
 *          ie a particular alignment.
 * attribs - array containing vertex attributes
 * num_attribs - number of attributes in a vertex, maximum TZ_MAX_ATTRIBUTES
 */
typedef struct
{
  size_t stride;

  tz_vertex_attrib_params attribs[TZ_MAX_ATTRIBUTES];
  size_t num_attribs;
} tz_buffer_format_params;
inline static tz_buffer_format_params tz_gen_buffer_format_params() { return (tz_buffer_format_params) { 0 }; }

typedef struct
{
  const char* name;
  const char* source;
  size_t size;
} tz_shader_stage_params;
inline static tz_shader_stage_params tz_gen_shader_stage_params() { return (tz_shader_stage_params) { 0 }; }

/* tz_shader - Struct defining a shader
*/
typedef struct
{
  tz_shader_stage_params vertex_shader;
  tz_shader_stage_params fragment_shader;
} tz_shader_params;
inline static tz_shader_params tz_gen_shader_params() { return (tz_shader_params) { 0 }; }

typedef struct
{
  tz_buffer_format_params buffer_attachment_formats[TZ_MAX_BUFFER_ATTACHMENTS];
  size_t num_buffer_attachments;

  tz_shader shader_program;
} tz_pipeline_params;
inline static tz_pipeline_params tz_gen_pipeline_params() { return (tz_pipeline_params) { { 0 }, 0, tz_pool_gen_invalid_id() }; }

typedef struct
{
  tz_buffer buffers[TZ_MAX_BUFFER_ATTACHMENTS];
  size_t num_buffers;

  tz_buffer index_buffer;
  tz_vertex_data_type index_type;
} tz_draw_resources;
inline static tz_draw_resources tz_gen_draw_resources() { return (tz_draw_resources) { { 0 }, 0, { tz_pool_gen_invalid_id() } }; }

typedef enum
{
  TZ_TRIANGLES,
  TZ_TRIANGLE_STRIP,
  TZ_LINES,
  TZ_POINTS
} tz_draw_type;

typedef struct
{
  tz_draw_type draw_type;
  size_t instances;
  size_t base_vertex;
  size_t num_vertices;
} tz_draw_call_params;
inline static tz_draw_call_params tz_gen_draw_call_params() { return (tz_draw_call_params) { 0 }; }

/* tz_gfx_device_config - Contains a description/settings for a gfx device
 */
typedef struct
{
  size_t max_shaders;
  size_t max_buffers;
  size_t max_pipelines;
} tz_gfx_device_resource_count;

typedef struct
{
  tz_gfx_device_resource_count resource_count;
  const tz_cb_allocator* allocator;

  enum
  {
    OPENGL
  } graphics_api;
} tz_gfx_device_params;
inline static tz_gfx_device_params tz_gen_device_params() { return (tz_gfx_device_params) { 0 }; }

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

tz_gfx_device_params tz_default_gfx_device_params();
/* tz_create_device - creates an opaque pointer to a tz_gfx_device
*  device_config - parameter struct containing the parameters of a device.
*                  default parameters will be used if this is NULL.
*/
#define TZ_GFX_CREATE_DEVICE(name) void name(tz_gfx_device* device, tz_gfx_device_params* device_config)
typedef TZ_GFX_CREATE_DEVICE(tz_create_device_f);
tz_gfx_device* tz_create_device(tz_gfx_device_params* device_config);

#define TZ_GFX_DELETE_DEVICE(name) void name(tz_gfx_device* device)
typedef TZ_GFX_DELETE_DEVICE(tz_delete_device_f);
TZ_GFX_DELETE_DEVICE(tz_delete_device);

#define TZ_GFX_CREATE_SHADER(name) tz_shader name(tz_gfx_device* device, const tz_shader_params* shader_create_info)
typedef TZ_GFX_CREATE_SHADER(tz_create_shader_f);
TZ_GFX_CREATE_SHADER(tz_create_shader);

#define TZ_GFX_DELETE_SHADER(name) void name(tz_gfx_device* device, tz_shader shader)
typedef TZ_GFX_DELETE_SHADER(tz_delete_shader_f);
TZ_GFX_DELETE_SHADER(tz_delete_shader);

#define TZ_GFX_CREATE_BUFFER(name) tz_buffer name(tz_gfx_device* device, const tz_buffer_params* buffer_create_info)
typedef TZ_GFX_CREATE_BUFFER(tz_create_buffer_f);
TZ_GFX_CREATE_BUFFER(tz_create_buffer);

#define TZ_GFX_DELETE_BUFFER(name) void name(tz_gfx_device* device, tz_buffer buffer)
typedef TZ_GFX_DELETE_BUFFER(tz_delete_buffer_f);
TZ_GFX_DELETE_BUFFER(tz_delete_buffer);

#define TZ_GFX_CREATE_PIPELINE(name) tz_pipeline name(tz_gfx_device* device, const tz_pipeline_params* pipeline_create_info)
typedef TZ_GFX_CREATE_PIPELINE(tz_create_pipeline_f);
TZ_GFX_CREATE_PIPELINE(tz_create_pipeline);

#define TZ_GFX_DELETE_PIPELINE(name) void name(tz_gfx_device* device, tz_pipeline pipeline)
typedef TZ_GFX_DELETE_PIPELINE(tz_delete_pipeline_f);
TZ_GFX_DELETE_PIPELINE(tz_delete_pipeline);

#define TZ_GFX_EXECUTE_DRAW_CALL(name) void name(tz_gfx_device* device, tz_pipeline pipeline_id, const tz_draw_resources* resources, const tz_draw_call_params* draw_call_params)
typedef TZ_GFX_EXECUTE_DRAW_CALL(tz_execute_draw_call_f);
TZ_GFX_EXECUTE_DRAW_CALL(tz_execute_draw_call);

#endif 
