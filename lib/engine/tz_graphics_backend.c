#include "tz_graphics_backend.h"
#include "tz_error.h"

#include <stdint.h>
#define DEVICE_ALIGN 16

typedef struct
{
  tz_create_device_f* create_device;
  tz_delete_device_f* delete_device;

  tz_create_shader_f* create_shader;
  tz_delete_shader_f* delete_shader;

  tz_create_buffer_f* create_buffer;
  tz_delete_buffer_f* delete_buffer;

  tz_create_pipeline_f* create_pipeline;
  tz_delete_pipeline_f* delete_pipeline;

  tz_execute_draw_call_f* execute_draw_call;
} tz_gfx_device_api;

struct tz_gfx_device
{
  tz_pool shader_pool;
  tz_pool buffer_pool;
  tz_pool pipeline_pool;

  tz_gfx_device_resource_count resource_count;
  tz_cb_allocator allocator;

  void* backend_data;
  tz_gfx_device_api api;
};

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>

typedef struct
{
  int location;
  size_t offset;
  GLenum data_type;
  int size;
  int divisor;
} tz_vertex_attrib_gl;

typedef struct
{
  size_t stride;

  tz_vertex_attrib_gl attribs[TZ_MAX_ATTRIBUTES];
  size_t num_attribs;
} tz_buffer_format_gl;

typedef struct
{
  tz_shader shader_program;
  tz_buffer_format_gl buffer_attachment_formats[TZ_MAX_BUFFER_ATTACHMENTS];
  size_t num_buffer_attachments;
} tz_pipeline_gl;

typedef struct
{
  GLuint* shader_programs;
  GLuint* buffers;
  tz_pipeline_gl* pipelines;

  GLuint dummy_vao;
} tz_gfx_device_gl;

static GLenum gl_buffer_binding(tz_buffer_binding binding)
{
  switch (binding)
  {
  case TZ_ARRAY_BUFFER:
    return GL_ARRAY_BUFFER;
  case TZ_ELEMENT_BUFFER:
    return GL_ELEMENT_ARRAY_BUFFER;
  default:
    TZ_ASSERT("Buffer type not supported\n");
  }
}

static GLenum gl_buffer_usage(tz_buffer_usage usage)
{
  switch (usage)
  {
  case TZ_STATIC_BUFFER:
    return GL_STATIC_DRAW;
  case TZ_DYNAMIC_BUFFER:
    return GL_DYNAMIC_DRAW;
  case TZ_STREAM_BUFFER:
    return GL_STREAM_DRAW;
  default:
    TZ_ASSERT("Buffer usage not supported\n");
  }
}

static GLenum gl_data_format(tz_vertex_data_type type)
{
  switch (type)
  {
  case TZ_FORMAT_BYTE:
    return GL_BYTE;
  case TZ_FORMAT_UBYTE:
    return GL_UNSIGNED_BYTE;
  case TZ_FORMAT_SHORT:
    return GL_SHORT;
  case TZ_FORMAT_USHORT:
    return GL_UNSIGNED_SHORT;
  case TZ_FORMAT_INT:
    return GL_INT;
  case TZ_FORMAT_UINT:
    return GL_UNSIGNED_INT;
  case TZ_FORMAT_HALF:
    return GL_HALF_FLOAT;
  case TZ_FORMAT_FLOAT:
    return GL_FLOAT;
  case TZ_FORMAT_DOUBLE:
    return GL_DOUBLE;
  case TZ_FORMAT_FIXED:
    return GL_FIXED;
  default:
    TZ_ASSERT("Data format not supported\n");
  }
}

TZ_GFX_CREATE_DEVICE(tz_create_device_gl)
{
  size_t total_size = sizeof(tz_gfx_device_gl) + DEVICE_ALIGN
    + device_config->resource_count.max_shaders * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_buffers * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_pipelines * sizeof(tz_pipeline) + DEVICE_ALIGN;

  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)TZ_ALLOC((*(device_config->allocator)), total_size, DEVICE_ALIGN);
  device_gl->shader_programs = (GLuint*)align_forward(device_gl + 1, DEVICE_ALIGN);
  device_gl->buffers = (GLuint*)align_forward(device_gl->shader_programs + sizeof(GLuint) * device_config->resource_count.max_shaders, DEVICE_ALIGN);
  device_gl->pipelines = (tz_pipeline_params*)align_forward(device_gl->buffers + sizeof(GLuint) * device_config->resource_count.max_buffers, DEVICE_ALIGN);
  device_gl->dummy_vao = 0;

  device->backend_data = device_gl;

  glGenVertexArrays(1, &device_gl->dummy_vao);
}

TZ_GFX_DELETE_DEVICE(tz_delete_device_gl)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  glDeleteVertexArrays(1, &device_gl->dummy_vao);

  TZ_FREE(device->allocator, device_gl);
}

static GLuint compile_shader(tz_gfx_device* device, GLenum type, const tz_shader_stage_params* shader_stage_create_info)
{
  if (!shader_stage_create_info->source)
    return 0;

  // Create shader
  GLuint shader_stage = glCreateShader(type);
  // Compile shader
  glShaderSource(shader_stage, 1, &shader_stage_create_info->source, (GLint*)&shader_stage_create_info->size);
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

TZ_GFX_CREATE_SHADER(tz_create_shader_gl)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  tz_shader shader_id = { tz_pool_gen_invalid_id() };

  GLuint vertex_shader = compile_shader(device, GL_VERTEX_SHADER, &shader_create_info->vertex_shader);
  GLuint fragment_shader = compile_shader(device, GL_FRAGMENT_SHADER, &shader_create_info->fragment_shader);

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
  glGetProgramiv(program, GL_LINK_STATUS, (int*)&linked);
  if (linked == GL_FALSE)
  {
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

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

  TZ_LOG("Shader successfully compiled and linked using %s and %s\n", shader_create_info->vertex_shader.name, shader_create_info->fragment_shader.name);

  shader_id.id = tz_pool_create_id(&device->shader_pool);

  if (!tz_pool_id_is_valid(&device->shader_pool, shader_id.id))
    return shader_id;

  device_gl->shader_programs[shader_id.id.index] = program;

  return shader_id;
}

TZ_GFX_DELETE_SHADER(tz_delete_shader_gl)
{
  if (!tz_pool_delete_id(&device->shader_pool, shader.id))
  {
    TZ_LOG("Shader deletion error: invalid ID.\n");
    return;
  }
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;

  glDeleteProgram(device_gl->shader_programs[shader.id.index]);
}

TZ_GFX_CREATE_BUFFER(tz_create_buffer_gl)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  tz_buffer buffer_id = TZ_INVALID_ID(tz_buffer);

  GLuint buf = 0;
  GLenum binding = GL_ARRAY_BUFFER; // This is set to GL_ARRAY_BUFFER because at this stage the binding doesn't matter. This is just to get the buffer bound.
  GLuint usage = gl_buffer_usage(buffer_create_info->usage);

  glGenBuffers(1, &buf);
  glBindBuffer(binding, buf);
  glBufferData(binding, buffer_create_info->size, buffer_create_info->data, usage);

  if (!buf)
  {
    TZ_LOG("Buffer creation error: could not create OpenGL buffer\n");
    return buffer_id;
  }

  buffer_id.id = tz_pool_create_id(&device->buffer_pool);

  if (!tz_pool_id_is_valid(&device->buffer_pool, buffer_id.id))
  {
    TZ_LOG("Buffer creation error: could not create ID. The pool may be full.\n");
    return buffer_id;
  }

  device_gl->buffers[buffer_id.id.index] = buf;

  glBindBuffer(binding, 0);

  return buffer_id;
}

TZ_GFX_DELETE_BUFFER(tz_delete_buffer_gl)
{
  if (!tz_pool_delete_id(&device->buffer_pool, buffer.id))
  {
    TZ_LOG("Buffer deletion error: invalid ID.\n");
    return;
  }

  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  glDeleteBuffers(1, &(device_gl->buffers[buffer.id.index]));
}

TZ_GFX_CREATE_PIPELINE(tz_create_pipeline_gl)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;
  tz_pipeline pipeline_id = { tz_pool_create_id(&device->pipeline_pool) };

  TZ_ASSERT(tz_pool_id_is_valid(&device->shader_pool, pipeline_create_info->shader_program.id), "Pipeline creation error: shader program ID invalid.");

  tz_pipeline_gl new_pipeline;
  new_pipeline.shader_program = pipeline_create_info->shader_program;
  new_pipeline.num_buffer_attachments = pipeline_create_info->num_buffer_attachments;

  //TODO: Pipeline validation, dummy draw call
  GLuint gl_program = device_gl->shader_programs[new_pipeline.shader_program.id.index];
  for (size_t buffer_index = 0; buffer_index < pipeline_create_info->num_buffer_attachments; buffer_index++)
  {
    tz_buffer_format_params* format_params = pipeline_create_info->buffer_attachment_formats + buffer_index;
    tz_buffer_format_gl* new_buffer_format = new_pipeline.buffer_attachment_formats + buffer_index;

    new_buffer_format->num_attribs = format_params->num_attribs;
    
    size_t packed_stride = 0;
    for (size_t attrib_index = 0; attrib_index < new_buffer_format->num_attribs; attrib_index++)
    {
      tz_vertex_attrib_params* attrib_params = format_params->attribs + attrib_index;

      tz_vertex_attrib_gl new_attrib;
      new_attrib.data_type = gl_buffer_binding(attrib_params->type);
      new_attrib.divisor = attrib_params->divisor;
      new_attrib.offset = attrib_params->offset;
      new_attrib.size = attrib_params->size;
      new_attrib.location = glGetAttribLocation(gl_program, attrib_params->name);

      new_buffer_format->attribs[attrib_index] = new_attrib;

      packed_stride += new_attrib.size;
    }

    if (format_params->stride == 0)
      new_buffer_format->stride = packed_stride;
    else
      new_buffer_format->stride = format_params->stride;
  }

  device_gl->pipelines[pipeline_id.id.index] = new_pipeline;

  return pipeline_id;
}

TZ_GFX_DELETE_PIPELINE(tz_delete_pipeline_gl)
{
  if (!tz_pool_delete_id(&device->pipeline_pool, pipeline.id))
    TZ_LOG("Pipeline deletion error: invalid ID.\n");
}

static void gl_set_attrib_pointers(GLuint vao, GLuint buffer, const tz_buffer_format_gl* buffer_format)
{
  glBindBuffer(GL_ARRAY_BUFFER, buffer);

  for (size_t attrib_index = 0; attrib_index < buffer_format->num_attribs; attrib_index++)
  {
    tz_vertex_attrib_gl* attrib = &buffer_format->attribs[attrib_index];
    glEnableVertexArrayAttrib(vao, attrib->location);
    glVertexAttribPointer(attrib->location, attrib->size, attrib->data_type, false, buffer_format->stride, attrib->offset);
    glVertexAttribDivisor(attrib->location, attrib->divisor);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static GLenum gl_draw_type(tz_draw_type type)
{
  switch (type)
  {
  case TZ_TRIANGLES:
    return GL_TRIANGLES;
  case TZ_TRIANGLE_STRIP:
    return GL_TRIANGLE_STRIP;
  case TZ_LINES:
    return GL_LINE;
  case TZ_POINTS:
    return GL_POINT;
  }
}

// TODO: should pass in a "command context" object to store current state, eventually wrap 
// everything in a command buffer
TZ_GFX_EXECUTE_DRAW_CALL(tz_execute_draw_call_gl)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*)device->backend_data;

  TZ_ASSERT(tz_pool_id_is_valid(&device->pipeline_pool, pipeline_id.id), "Pipeline creation error: pipeline ID invalid.");
  tz_pipeline_gl* pipeline = &device_gl->pipelines[pipeline_id.id.index];

  // TEMP: invariant, number of buffer attachments should equal the number of
  // buffer formats in the pipeline. Need some better way to enforce this, like
  // initializing the resource set with a function that requires a pipeline object
  // to check the resources
  TZ_ASSERT(pipeline->num_buffer_attachments == resources->num_buffers, "Resources and pipeline don't match");

  TZ_ASSERT(tz_pool_id_is_valid(&device->shader_pool, pipeline->shader_program.id), "Pipeline creation error: shader program ID invalid.");
  GLuint shader_program = device_gl->shader_programs[pipeline->shader_program.id.index];

  // Set attribute pointers
  for (size_t buffer_index = 0; buffer_index < pipeline->num_buffer_attachments; buffer_index++)
  {
    TZ_ASSERT(tz_pool_id_is_valid(&device->buffer_pool, resources->buffers[buffer_index].id), "Pipeline creation error: buffer ID invalid.");
    GLuint buffer = device_gl->buffers[resources->buffers[buffer_index].id.index];

    gl_set_attrib_pointers(device_gl->dummy_vao, buffer, pipeline->buffer_attachment_formats + buffer_index);
  }

  glUseProgram(shader_program);

  if (tz_pool_id_is_valid(&device->buffer_pool, resources->index_buffer.id))
  {
    GLuint index_buffer = device_gl->buffers[resources->index_buffer.id.index];
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glDrawElementsInstancedBaseVertex(gl_draw_type(draw_call_params->draw_type), draw_call_params->num_vertices, gl_data_format(resources->index_type), NULL, draw_call_params->instances, draw_call_params->base_vertex);
  }
  else
  {
    glDrawArraysInstanced(gl_draw_type(draw_call_params->draw_type), draw_call_params->base_vertex, draw_call_params->num_vertices, draw_call_params->instances);
  }
}

tz_gfx_device_api opengl_api = {
  .create_device = tz_create_device_gl,
  .delete_device = tz_delete_device_gl,

  .create_shader = tz_create_shader_gl,
  .delete_shader = tz_delete_shader_gl,

  .create_buffer = tz_create_buffer_gl,
  .delete_buffer = tz_delete_buffer_gl,

  .create_pipeline = tz_create_pipeline_gl,
  .delete_pipeline = tz_delete_pipeline_gl,

  .execute_draw_call = tz_execute_draw_call_gl
};

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

tz_gfx_device_params tz_default_gfx_device_params()
{
  return (tz_gfx_device_params) {
    .resource_count = {
      .max_shaders = 100,
      .max_buffers = 100,
      .max_pipelines = 100,
    },
    .allocator = tz_default_cb_allocator(),
    .graphics_api = OPENGL
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
  tz_create_pool(&device->pipeline_pool, device_config->resource_count.max_pipelines, device_config->allocator);

  switch (device_config->graphics_api)
  {
  case OPENGL:
    device->api = opengl_api;
  default:
    device->api = opengl_api;
  }

  device->api.create_device(device, device_config);

  return device;
}

TZ_GFX_DELETE_DEVICE(tz_delete_device)
{
  device->api.delete_device(device);

  tz_delete_pool(&(device->shader_pool));
  tz_delete_pool(&(device->buffer_pool));
  tz_delete_pool(&(device->pipeline_pool));

  TZ_FREE(device->allocator, device);
}

TZ_GFX_CREATE_SHADER(tz_create_shader)
{
  return device->api.create_shader(device, shader_create_info);
}

TZ_GFX_DELETE_SHADER(tz_delete_shader)
{
  device->api.delete_shader(device, shader);
}

TZ_GFX_CREATE_BUFFER(tz_create_buffer)
{
  return device->api.create_buffer(device, buffer_create_info);
}

TZ_GFX_DELETE_BUFFER(tz_delete_buffer)
{
  device->api.delete_buffer(device, buffer);
}

TZ_GFX_CREATE_PIPELINE(tz_create_pipeline)
{
  return device->api.create_pipeline(device, pipeline_create_info);
}

TZ_GFX_DELETE_PIPELINE(tz_delete_pipeline)
{
  device->api.delete_pipeline(device, pipeline);
}

TZ_GFX_EXECUTE_DRAW_CALL(tz_execute_draw_call)
{
  device->api.execute_draw_call(device, pipeline_id, resources, draw_call_params);
}
