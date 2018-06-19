#include "tz_graphics_backend.h"
#include "tz_error.h"

#include <stdint.h>
#include <tinycthread.h>

tz_create_device_f* tz_api_create_device;
tz_delete_device_f* tz_api_delete_device;

tz_create_shader_f* tz_create_shader;
tz_delete_shader_f* tz_delete_shader;

tz_create_buffer_f* tz_create_buffer;
tz_delete_buffer_f* tz_delete_buffer;

tz_create_pipeline_f* tz_create_pipeline;
tz_delete_pipeline_f* tz_delete_pipeline;

tz_execute_draw_groups_f* tz_execute_draw_groups;

tz_gfx_handle tz_gfx_invalid_handle;

typedef struct 
{
  tz_gfx_device_resource_count resource_count;
  tz_allocator allocator;
} tz_gfx_device;

static tz_gfx_device g_device;

static size_t tz_vertex_data_type_bytes(tz_vertex_data_type type)
{
  switch (type)
  {
  case TZ_FORMAT_BYTE:
  case TZ_FORMAT_UBYTE:
    return sizeof(uint8_t);
  case TZ_FORMAT_SHORT:
  case TZ_FORMAT_USHORT:
    return sizeof(uint16_t);
  case TZ_FORMAT_INT:
  case TZ_FORMAT_UINT:
    return sizeof(uint32_t);
  case TZ_FORMAT_HALF:
    return sizeof(float) / 2;
  case TZ_FORMAT_FLOAT:
    return sizeof(float);
  default:
    return 0;
  }
}

typedef struct
{
  tz_pipeline pipeline;
  tz_draw_resources resources;
} tz_draw_state;

////////////////////////////////////////////////////////////////////////////////
// COMMAND BUFFER
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  tz_arena memory;
  tz_draw_resources current_resources;
  tz_pipeline current_pipeline;

  bool submitting;
  cnd_t cond;
  mtx_t mtx;

  bool recording;
} _tz_command_buffer;

typedef struct
{
  tz_buffer buffer;
  size_t binding;
} tz_command_bind_vertex_buffer;

typedef struct
{
  tz_buffer buffer;
} tz_command_bind_index_buffer;

typedef struct
{
  tz_pipeline pipeline;
} tz_command_bind_pipeline;

typedef struct
{
  tz_draw_call_params draw_call_params;
} tz_command_draw;

typedef struct
{
  enum
  {
    BIND_VERTEX_BUFFER,
    BIND_INDEX_BUFFER,
    BIND_PIPELINE,
    DRAW
  } type;
  union
  {
    tz_command_bind_vertex_buffer bind_vb;
    tz_command_bind_index_buffer bind_ib;
    tz_command_bind_pipeline bind_pipeline;
    tz_draw_call_params draw;
  };
} tz_command;

void tz_cb_create(tz_gfx_device *device, _tz_command_buffer *cb, size_t block_size)
{
  cb->submitting = false;
  cnd_init(&cb->cond);
  mtx_init(&cb->mtx, mtx_plain);

  cb->recording = false;

  cb->memory = TZ_ALLOC_ARENA(device->allocator, block_size, TZ_DEFAULT_ALIGN);
}

void tz_cb_destroy(tz_gfx_device *device, _tz_command_buffer *cb)
{
  cnd_destroy(&cb->cond);
  mtx_destroy(&cb->mtx);

  TZ_FREE_ARENA(cb->memory);
}

void tz_cb_begin(_tz_command_buffer *cb)
{
  // Attempt to pass submission fence, ie: do not begin recording until submission involving this command buffer is completed
  mtx_lock(&cb->mtx);
  while (cb->submitting)
  {
    cnd_wait(&cb->cond, &cb->mtx);
  }

  cb->recording = true;
}

void tz_cb_end(_tz_command_buffer *cb)
{
  // Block any operations until submission is complete
  cb->recording = false;
  mtx_unlock(&cb->mtx);

}

void tz_cb_bind_vertex_buffer(_tz_command_buffer *cb, tz_buffer buffer, size_t binding)
{
  if (!cb->recording)
    return;
}

void tz_cb_bind_index_buffer(_tz_command_buffer *cb, tz_buffer buffer)
{
  if (!cb->recording)
    return;
}

void tz_cb_bind_pipeline(_tz_command_buffer *cb, tz_pipeline pipeline)
{
  if (!cb->recording)
    return;
}

void tz_cb_draw(_tz_command_buffer *cb, const tz_draw_call_params *draw_call_params)
{
  if (!cb->recording)
    return;
}

void tz_cb_reset(_tz_command_buffer *cb)
{
  if (!cb->recording)
    return;

  mtx_lock(&cb->mtx);
  while (cb->submitting)
  {
    cnd_wait(&cb->cond, &cb->mtx);
  }

  // Reset command buffer

  mtx_unlock(&cb->mtx);
}

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#define TZ_GL_INVALID_ID 0
#define TZ_GL_HANDLE(handle) ((GLuint) handle.data)

typedef struct
{
  int location;
  size_t buffer_binding;
  size_t offset;
  GLenum data_type;
  int size;
  int divisor;
} tz_vertex_attrib_gl;

typedef struct
{
  tz_shader shader_program;
  tz_vertex_attrib_gl attribs[TZ_MAX_ATTRIBUTES];
  size_t num_attribs;
  size_t stride;
} tz_pipeline_gl;

TZ_POOL_VECTOR(gl_pipeline, tz_pipeline_gl)

typedef struct
{
  tz_gl_pipeline_pool pipelines;
  GLuint dummy_vao;
} tz_gfx_device_gl;

static tz_gfx_device_gl g_device_gl;

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
  default:
    TZ_ASSERT(false, "Data format not supported\n");
  }
}

TZ_GFX_CREATE_DEVICE(tz_create_device_gl3)
{
  tz_create_gl_pipeline_pool(&g_device_gl.pipelines, device_config->resource_count.max_pipelines, device_config->allocator); 
  g_device_gl.dummy_vao = 0;

  glGenVertexArrays(1, &g_device_gl.dummy_vao);
  glBindVertexArray(g_device_gl.dummy_vao);
}

TZ_GFX_DELETE_DEVICE(tz_delete_device_gl3)
{
  glDeleteVertexArrays(1, &g_device_gl.dummy_vao);
}

static GLuint compile_shader(const tz_allocator* allocator, GLenum type, const tz_shader_stage_params *shader_stage_create_info)
{
  if (!shader_stage_create_info->source)
    return 0;

  // Create shader
  GLuint shader_stage = glCreateShader(type);
  // Compile shader
  glShaderSource(shader_stage, 1, &shader_stage_create_info->source, (GLint *)&shader_stage_create_info->size);
  // Compile the vertex shader
  glCompileShader(shader_stage);

  // Check for errors
  GLint compiled = 0;
  glGetShaderiv(shader_stage, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_FALSE)
  {
    GLint len = 0;
    glGetShaderiv(shader_stage, GL_INFO_LOG_LENGTH, &len);

    char *error_buffer = TZ_ALLOC((*allocator), sizeof(char) * len, TZ_DEFAULT_ALIGN);
    glGetShaderInfoLog(shader_stage, len, &len, error_buffer);

    TZ_LOG("Shader compilation error in %s: %s\n", shader_stage_create_info->name, error_buffer);

    TZ_FREE((*allocator), error_buffer);
    glDeleteShader(shader_stage);

    // Return invalid id if error
    return 0;
  }

  // Add to pool
  return shader_stage;
}

TZ_GFX_CREATE_SHADER(tz_create_shader_gl3)
{
  tz_shader shader_id = { TZ_GL_INVALID_ID };

  GLuint vertex_shader = compile_shader(&g_device.allocator, GL_VERTEX_SHADER, &shader_create_info->vertex_shader);
  GLuint fragment_shader = compile_shader(&g_device.allocator, GL_FRAGMENT_SHADER, &shader_create_info->fragment_shader);

  if (!vertex_shader || !fragment_shader)
  {
    return shader_id;
  }

  GLuint program = glCreateProgram();

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);

  glLinkProgram(program);

  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, (int *)&linked);
  if (linked == GL_FALSE)
  {
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

    char *error_buffer = TZ_ALLOC(g_device.allocator, sizeof(char) * len, TZ_DEFAULT_ALIGN);
    glGetProgramInfoLog(program, len, &len, error_buffer);

    TZ_LOG("Shader linking error: %s\n", error_buffer);

    TZ_FREE(g_device.allocator, error_buffer);
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

  shader_id = (tz_shader) { program };

  return shader_id;
}

TZ_GFX_DELETE_SHADER(tz_delete_shader_gl3)
{
  glDeleteProgram((GLuint) shader.data);
}

TZ_GFX_CREATE_BUFFER(tz_create_buffer_gl3)
{
  tz_buffer buffer_id = { TZ_GL_INVALID_ID };

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

  buffer_id = (tz_buffer) { buf };

  glBindBuffer(binding, 0);

  return buffer_id;
}

TZ_GFX_DELETE_BUFFER(tz_delete_buffer_gl3)
{
  GLuint handle = (GLuint) buffer.data;
  glDeleteBuffers(1, &handle);
}

TZ_GFX_CREATE_PIPELINE(tz_create_pipeline_gl3)
{
  tz_pipeline_gl new_pipeline = {0};
  new_pipeline.shader_program = pipeline_create_info->shader_program;

  //TODO: Pipeline validation, dummy draw call
  GLuint gl_program = TZ_GL_HANDLE(new_pipeline.shader_program);

  size_t packed_stride = 0;
  for (size_t attrib_index = 0, gl_attrib_index = 0; attrib_index < TZ_MAX_ATTRIBUTES && gl_attrib_index < TZ_MAX_ATTRIBUTES; attrib_index++, gl_attrib_index++)
  {
    const tz_vertex_attrib_params *attrib_params = pipeline_create_info->vertex_attribs + attrib_index;
    if (attrib_params->type == TZ_FORMAT_INVALID)
      continue;

    if (attrib_params->buffer_binding > TZ_MAX_BUFFER_ATTACHMENTS)
    {
      gl_attrib_index--;
      continue;
    }

    tz_vertex_attrib_gl new_attrib;
    new_attrib.data_type = gl_data_format(attrib_params->type);
    new_attrib.divisor = attrib_params->divisor;
    new_attrib.offset = attrib_params->offset;
    new_attrib.size = attrib_params->size;
    new_attrib.buffer_binding = attrib_params->buffer_binding;

    if (attrib_params->name)
      new_attrib.location = glGetAttribLocation(gl_program, attrib_params->name);
    else
      new_attrib.location = attrib_index;

    new_pipeline.attribs[gl_attrib_index] = new_attrib;
    new_pipeline.num_attribs++;

    packed_stride += new_attrib.size * tz_vertex_data_type_bytes(attrib_params->type);
  }

  if (pipeline_create_info->stride == 0)
    new_pipeline.stride = packed_stride;
  else
    new_pipeline.stride = pipeline_create_info->stride;

  return (tz_pipeline) { tz_gl_pipeline_pool_push(&g_device_gl.pipelines, new_pipeline) };
}

TZ_GFX_DELETE_PIPELINE(tz_delete_pipeline_gl3)
{
  if (!tz_gl_pipeline_pool_delete(&g_device_gl.pipelines, pipeline.data))
    TZ_LOG("Pipeline deletion error: invalid ID.\n");
}

static void gl3_clear_pipeline(tz_pipeline pipeline_id)
{
  tz_pipeline_gl* pipeline_gl = tz_gl_pipeline_pool_get_p(&g_device_gl.pipelines, pipeline_id.data);

  for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
  {
    tz_vertex_attrib_gl *attrib = &pipeline_gl->attribs[attrib_index];
    glDisableVertexAttribArray(attrib->location);
  }

  glUseProgram(0);
}

static void gl3_bind_pipeline(tz_draw_state *current_draw_state, const tz_pipeline pipeline_id)
{
  tz_pipeline_gl* pipeline_gl = tz_gl_pipeline_pool_get_p(&g_device_gl.pipelines, pipeline_id.data);
  current_draw_state->pipeline = pipeline_id;
  gl3_clear_pipeline(pipeline_id);

  TZ_ASSERT(TZ_GFX_HANDLE_IS_VALID(pipeline_gl->shader_program),
            "Pipeline creation error: shader program ID invalid.");
  GLuint shader_program = TZ_GL_HANDLE(pipeline_gl->shader_program);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(shader_program);
}

static void gl3_bind_resources(tz_pipeline pipeline_id, tz_draw_state *current_draw_state, const tz_draw_resources *draw_resources)
{
  tz_pipeline_gl *pipeline_gl = tz_gl_pipeline_pool_get_p(&g_device_gl.pipelines, pipeline_id.data);

  for (size_t attrib_index = 0; attrib_index < pipeline_gl->num_attribs; attrib_index++)
  {
    GLuint current_buffer = 0;
    tz_vertex_attrib_gl *attrib = &pipeline_gl->attribs[attrib_index];

    tz_buffer buffer = draw_resources->buffers[attrib->buffer_binding];
    TZ_ASSERT(TZ_GFX_HANDLE_IS_VALID(buffer),
              "Buffer at binding %lu was invalid", attrib->buffer_binding);

    if (current_draw_state->resources.buffers[attrib->buffer_binding].data == buffer.data)
      continue;

    current_draw_state->resources.buffers[attrib->buffer_binding] = buffer;

    if (current_buffer != TZ_GL_HANDLE(buffer))
    {
      current_buffer = TZ_GL_HANDLE(buffer);
      glBindBuffer(GL_ARRAY_BUFFER, current_buffer);
    }

    glVertexAttribPointer(attrib->location,
                          attrib->size,
                          attrib->data_type,
                          GL_FALSE,
                          pipeline_gl->stride,
                          (GLvoid *)attrib->offset);
    glVertexAttribDivisor(attrib->location, attrib->divisor);
    glEnableVertexAttribArray(attrib->location);
  }

  if (current_draw_state->resources.index_buffer.data != draw_resources->index_buffer.data && TZ_GFX_HANDLE_IS_VALID(current_draw_state->resources.index_buffer))
  {
    current_draw_state->resources.index_buffer = draw_resources->index_buffer;
    current_draw_state->resources.index_type = draw_resources->index_type;

    GLuint index_buffer = TZ_GL_HANDLE(current_draw_state->resources.index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
  }
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
TZ_GFX_EXECUTE_DRAW_GROUPS(tz_execute_draw_groups_gl3)
{
  tz_draw_state current_draw_state = {0};
  for (size_t i = 0; i < num_draw_groups; i++)
  {
    tz_pipeline pipeline = draw_groups[i].pipeline;
    const tz_draw_resources *resources = &draw_groups[i].resources;

    TZ_ASSERT(tz_gl_pipeline_pool_index_is_valid(&g_device_gl.pipelines, pipeline.data),
              "Pipeline binding error: pipeline ID invalid.");

    gl3_bind_pipeline(&current_draw_state, pipeline);

    const tz_draw_group *draw_group = &draw_groups[i];
    for (size_t j = 0; j < draw_group->num_draw_calls; j++)
    {
      const tz_draw_call_params *draw_call = &(draw_group->draw_calls[j]);
      gl3_bind_resources(pipeline, &current_draw_state, resources);

      if (TZ_GFX_HANDLE_IS_VALID(resources->index_buffer))
      {
        glDrawElementsInstancedBaseVertex(gl_draw_type(draw_call->draw_type),
                                          draw_call->num_vertices,
                                          gl_data_format(resources->index_type),
                                          NULL,
                                          draw_call->instances,
                                          draw_call->base_vertex);
      }
      else
      {
        glDrawArraysInstanced(gl_draw_type(draw_call->draw_type),
                              draw_call->base_vertex,
                              draw_call->num_vertices,
                              draw_call->instances);
      }
    }
  }
  gl3_clear_pipeline(current_draw_state.pipeline);
}

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

tz_gfx_device_params tz_default_gfx_device_params()
{
  return (tz_gfx_device_params){
      .resource_count = {
          .max_shaders = 256,
          .max_buffers = 256,
          .max_pipelines = 256,
          .max_command_buffers = 256},
      .allocator = tz_default_cb_allocator(),
      .graphics_api = OPENGL3};
}

void tz_gfx_init(tz_gfx_device_params *device_config)
{
  if (device_config == NULL)
  {
    static tz_gfx_device_params default_device_config = {0};
    default_device_config = tz_default_gfx_device_params();
    device_config = &default_device_config;
  }

  device_config->allocator = device_config->allocator ? device_config->allocator : tz_default_cb_allocator();

  g_device.allocator = *(device_config->allocator);
  g_device.resource_count = device_config->resource_count;

  switch (device_config->graphics_api)
  {
  case OPENGL3:
    tz_api_create_device = tz_create_device_gl3;
    tz_api_delete_device = tz_delete_device_gl3;

    tz_create_shader = tz_create_shader_gl3;
    tz_delete_shader = tz_delete_shader_gl3;

    tz_create_buffer = tz_create_buffer_gl3;
    tz_delete_buffer = tz_delete_buffer_gl3;

    tz_create_pipeline = tz_create_pipeline_gl3;
    tz_delete_pipeline = tz_delete_pipeline_gl3;

    tz_execute_draw_groups = tz_execute_draw_groups_gl3;

    tz_gfx_invalid_handle = TZ_GL_INVALID_ID;
    break;
  }

  tz_api_create_device(device_config);
}

void tz_gfx_destroy()
{
  tz_api_delete_device();
}