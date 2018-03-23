#include "graphics_backend.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// UTILS 
////////////////////////////////////////////////////////////////////////////////

inline static void* align_forward(void* ptr, size_t align)
{
  uintptr_t uint_ptr = (uintptr_t) ptr;
  return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

static void* tz_malloc(void* user_data, size_t size, size_t align)
{
  size_t offset = align + sizeof(void*);
  void* ptr = malloc(size + offset);

  if (ptr == NULL)
    return NULL;

  void** return_ptr = (void**) align_forward(((void**) ptr) + 1, align);
  return_ptr[-1] = ptr;

  return (void*) return_ptr;
}

static void* tz_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
  size_t offset = align + sizeof(void*);
  void* new_ptr = realloc(ptr, size + offset);

  if (new_ptr == NULL)
    return NULL;

  void** return_ptr = (void**) align_forward(((void**) new_ptr) + 1, align);
  return_ptr[-1] = new_ptr;

  return (void*) return_ptr;
}

static void tz_free(void* user_data, void* ptr)
{
  free(((void**) ptr)[-1]);
}

const tz_cb_allocator* tz_default_cb_allocator()
{
  static tz_cb_allocator* p_allocator = NULL;
  static tz_cb_allocator allocator;

  if (p_allocator == NULL)
  {
    allocator.alloc = tz_malloc;
    allocator.realloc = tz_realloc;
    allocator.dealloc = tz_free;
    allocator.user_data = NULL;
    
    p_allocator = &allocator;
  }

  return p_allocator;
}

////////////////////////////////////////////////////////////////////////////////
// DISPATCH FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

// OPENGL
void tz_create_device_gl(tz_gfx_device* device, const tz_gfx_device_params* device_config);
void tz_delete_device_gl(tz_gfx_device* device);
tz_shader_stage tz_create_shader_stage_gl(tz_gfx_device* device, const tz_shader_stage_params* shader_stage_create_info);

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

#define DEVICE_ALIGN 16

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_cb_allocator* allocator)
{
  pool->allocator = *allocator;
  pool->free_indices = (uint32_t*) TZ_ALLOC(pool->allocator, sizeof(uint32_t) * capacity, 4);
  pool->capacity = capacity;
  pool->num_free_indices = capacity;

  for (size_t i = 0; i < capacity; i++)
    pool->free_indices[i] = i;
}

void tz_delete_pool(tz_pool* pool)
{
  TZ_FREE(pool->allocator, pool->free_indices);
}

tz_pool_index tz_pool_create_id(tz_pool* pool)
{
  if (pool->num_free_indices == 0)
    return TZ_POOL_INVALID_INDEX;

  return pool->free_indices[--pool->num_free_indices];
}

void tz_pool_delete_id(tz_pool* pool, tz_pool_index index)
{
  if (index == TZ_POOL_INVALID_INDEX)
    return;

  if (pool->num_free_indices >= pool->capacity)
    return;

  pool->free_indices[pool->num_free_indices++] = index;
}

struct tz_gfx_device
{
  tz_pool shader_stage_pool;
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

  tz_delete_pool(&(device->shader_stage_pool));
  tz_delete_pool(&(device->shader_pool));
  tz_delete_pool(&(device->buffer_pool));
  tz_delete_pool(&(device->vertex_format_pool));
  tz_delete_pool(&(device->pipeline_pool));

  TZ_FREE(device->allocator, device);
}

tz_gfx_device* tz_create_device(const tz_gfx_device_params* device_config)
{
  tz_gfx_device* device = TZ_ALLOC((*(device_config->allocator)), sizeof(tz_gfx_device), DEVICE_ALIGN);
  device->allocator = *(device_config->allocator);
  device->resource_count = device_config->resource_count;

  tz_create_pool(&device->shader_stage_pool, device_config->resource_count.max_shader_stages, device_config->allocator);
  tz_create_pool(&device->shader_pool, device_config->resource_count.max_shaders, device_config->allocator);
  tz_create_pool(&device->buffer_pool, device_config->resource_count.max_buffers, device_config->allocator);
  tz_create_pool(&device->vertex_format_pool, device_config->resource_count.max_vertex_formats, device_config->allocator);
  tz_create_pool(&device->pipeline_pool, device_config->resource_count.max_pipelines, device_config->allocator);

  // TEMPORARY, this function should dispatch dynamically to whatever api is selected
  tz_create_device_gl(device, device_config);

  return device;
}

// TEMPORARY, this function should dispatch dynamically to whatever api is selected
tz_shader_stage tz_create_shader_stage(tz_gfx_device* device, const tz_shader_stage_params* shader_stage_create_info)
{
  return tz_create_shader_stage_gl(device, shader_stage_create_info);
}

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>

typedef struct 
{
  GLuint* shader_stages;
  GLuint* shader_programs;
  GLuint* buffers;
  tz_vertex_format_params* vertex_formats; 
  tz_pipeline_params* pipelines;

} tz_gfx_device_gl;

void tz_create_device_gl(tz_gfx_device* device, const tz_gfx_device_params* device_config)
{
  size_t total_size = sizeof(tz_gfx_device_gl) + DEVICE_ALIGN
    + device_config->resource_count.max_shader_stages * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_shaders * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_buffers * sizeof(GLuint) + DEVICE_ALIGN
    + device_config->resource_count.max_vertex_formats * sizeof(tz_vertex_format) + DEVICE_ALIGN
    + device_config->resource_count.max_pipelines * sizeof(tz_pipeline) + DEVICE_ALIGN;

  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*) TZ_ALLOC((*(device_config->allocator)), total_size, DEVICE_ALIGN);
  device_gl->shader_stages = (GLuint*) align_forward(device + 1, DEVICE_ALIGN);
  device_gl->shader_programs = (GLuint*) align_forward(device_gl->shader_stages + device_config->resource_count.max_shader_stages, DEVICE_ALIGN);
  device_gl->buffers = (GLuint*) align_forward(device_gl->shader_programs + device_config->resource_count.max_shaders, DEVICE_ALIGN);
  device_gl->vertex_formats = (tz_vertex_format_params*) align_forward(device_gl->buffers + device_config->resource_count.max_buffers, DEVICE_ALIGN);
  device_gl->pipelines = (tz_pipeline_params*) align_forward(device_gl->vertex_formats + device_config->resource_count.max_vertex_formats, DEVICE_ALIGN);

  device->backend_data = device_gl;
}

void tz_delete_device_gl(tz_gfx_device* device)
{
  TZ_FREE(device->allocator, device->backend_data);
}

tz_shader_stage tz_create_shader_stage_gl(tz_gfx_device* device, const tz_shader_stage_params* shader_stage_create_info)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*) device->backend_data;
}
