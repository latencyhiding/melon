#include "graphics_backend.h"

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// DISPATCH FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

// OPENGL
void tz_create_device_gl(tz_gfx_device* device, const tz_gfx_device_params* device_config);
void tz_delete_device_gl(tz_gfx_device* device);
tz_shader tz_create_shader_gl(tz_gfx_device* device, const tz_shader_params* shader_create_info);

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

#define DEVICE_ALIGN 16

struct tz_gfx_device
{
  tz_pool shader_pool;
  tz_pool buffer_pool;
  tz_pool vertex_format_pool;
  tz_pool pipeline_pool; 

  tz_gfx_device_resource_count resource_count;
  tz_cb_allocator allocator;
  void* backend_data;
};

void tz_delete_device(tz_gfx_device* device)
{
  // TEMPORARY, this function should dispatch dynamically to whatever api is selected
  tz_delete_device_gl(device);

  tz_delete_pool(&(device->shader_pool));
  tz_delete_pool(&(device->buffer_pool));
  tz_delete_pool(&(device->vertex_format_pool));
  tz_delete_pool(&(device->pipeline_pool));

  TZ_FREE(device->allocator, device);
}

tz_gfx_device_params tz_default_gfx_device_params()
{
  return (tz_gfx_device_params) {
    .resource_count = {
      .max_shaders = 100,
      .max_buffers = 100,
      .max_vertex_formats = 100,
      .max_pipelines = 100,
    },
    .allocator = tz_default_cb_allocator(),
  };
}

tz_gfx_device* tz_create_device(tz_gfx_device_params* device_config)
{
  if (device_config == NULL)
  {
    static tz_gfx_device_params default_device_config = { 0 };
    default_device_config = tz_default_gfx_device_params();
    device_config = &default_device_config;
  }

  device_config->allocator = device_config->allocator ? device_config->allocator : tz_default_cb_allocator();

  tz_gfx_device* device = TZ_ALLOC((*(device_config->allocator)), sizeof(tz_gfx_device), DEVICE_ALIGN);
  device->allocator = *(device_config->allocator);
  device->resource_count = device_config->resource_count;

  tz_create_pool(&device->shader_pool, device_config->resource_count.max_shaders, device_config->allocator);
  tz_create_pool(&device->buffer_pool, device_config->resource_count.max_buffers, device_config->allocator);
  tz_create_pool(&device->vertex_format_pool, device_config->resource_count.max_vertex_formats, device_config->allocator);
  tz_create_pool(&device->pipeline_pool, device_config->resource_count.max_pipelines, device_config->allocator);

  // TEMPORARY, this function should dispatch dynamically to whatever api is selected
  tz_create_device_gl(device, device_config);

  return device;
}

// TEMPORARY, this function should dispatch dynamically to whatever api is selected
tz_shader tz_create_shader(tz_gfx_device* device, const tz_shader_params* shader_create_params)
{
  return tz_create_shader_gl(device, shader_create_params);
}

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>

typedef struct
{
  GLuint* shader_programs;
  GLuint* buffers;
  tz_vertex_format_params* vertex_formats;
  tz_pipeline_params* pipelines;

} tz_gfx_device_gl;

static tz_shader tz_gfx_device_add_shader_gl(tz_gfx_device* device, GLuint program)
{
  tz_shader shader_id = { tz_pool_create_id(&device->shader_pool) };

  if (shader_id.index = TZ_POOL_INVALID_INDEX)
    return shader_id;

  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  device_gl->shader_programs[shader_id.index] = program;

  return shader_id;
}

void tz_create_device_gl(tz_gfx_device* device, const tz_gfx_device_params* device_config)
{
  size_t total_size = sizeof(tz_gfx_device_gl) + DEVICE_ALIGN
    + device_config->resource_count.max_shaders * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_buffers * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_vertex_formats * sizeof(tz_vertex_format) + DEVICE_ALIGN
    + device_config->resource_count.max_pipelines * sizeof(tz_pipeline) + DEVICE_ALIGN;

  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)TZ_ALLOC((*(device_config->allocator)), total_size, DEVICE_ALIGN);
  device_gl->shader_programs = (GLuint*)align_forward(device_gl + 1, DEVICE_ALIGN);
  device_gl->buffers = (GLuint*)align_forward(device_gl->shader_programs + device_config->resource_count.max_shaders, DEVICE_ALIGN);
  device_gl->vertex_formats = (tz_vertex_format_params*)align_forward(device_gl->buffers + device_config->resource_count.max_buffers, DEVICE_ALIGN);
  device_gl->pipelines = (tz_pipeline_params*)align_forward(device_gl->vertex_formats + device_config->resource_count.max_vertex_formats, DEVICE_ALIGN);

  device->backend_data = device_gl;
}

void tz_delete_device_gl(tz_gfx_device* device)
{
  TZ_FREE(device->allocator, device->backend_data);
}

static GLuint compile_shader(tz_gfx_device* device, GLenum type, const tz_shader_stage_params* shader_stage_create_info)
{
  // Create shader
  GLuint shader_stage = glCreateShader(type);
  // Compile shader
  glShaderSource(shader_stage, 1, &shader_stage_create_info->source, (GLint*) &shader_stage_create_info->size);
  // Compile the vertex shader
  glCompileShader(shader_stage);

  // Check for errors
  GLint compiled = 0;
  glGetShaderiv(shader_stage, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_FALSE)
  {
    GLint len = 0;
    glGetShaderiv(shader_stage, GL_INFO_LOG_LENGTH, &len);

    char* error_buffer = TZ_ALLOC(device->allocator, sizeof(char) * len, DEVICE_ALIGN);
    glGetShaderInfoLog(shader_stage, len, &len, error_buffer);

    TZ_LOG("Shader compilation error in %s: %s\n", shader_stage_create_info->name, error_buffer);

    TZ_FREE(device->allocator, error_buffer);
    glDeleteShader(shader_stage);

    // Return invalid id if error
    return 0;
  }

  // Add to pool
  return shader_stage;
}

tz_shader tz_create_shader_gl(tz_gfx_device* device, const tz_shader_params* shader_create_info)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  tz_shader shader_id = { TZ_POOL_INVALID_INDEX };
  
  GLuint vertex_shader = compile_shader(device, GL_VERTEX_SHADER, &shader_create_info->vertex_shader);
  GLuint fragment_shader = compile_shader(device, GL_VERTEX_SHADER, &shader_create_info->fragment_shader);

  if (!vertex_shader
    || !fragment_shader)
  {
    return shader_id;
  }

  GLuint program = glCreateProgram();

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);

  glLinkProgram(program);

  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, (int*) &program);
  if (linked == GL_FALSE)
  {
    GLint len = 0;
    glGetProgramiv(linked, GL_INFO_LOG_LENGTH, &len);

    char* error_buffer = TZ_ALLOC(device->allocator, sizeof(char) * len, DEVICE_ALIGN);
    glGetProgramInfoLog(program, len, &len, error_buffer);

    TZ_LOG("Shader linking error: %s\n", error_buffer);

    TZ_FREE(device->allocator, error_buffer);
    glDeleteProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Return invalid id if error
    return shader_id;
  }
  
  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return tz_gfx_device_add_shader_gl(device, program);
}

void tz_delete_shader(tz_gfx_device* device, tz_shader shader)
{
  tz_pool_delete_id(&device->shader_pool, shader.index);
}
