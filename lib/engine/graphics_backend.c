#include "graphics_backend.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

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

tz_cb_allocator tz_default_cb_allocator()
{
  tz_cb_allocator allocator;
  allocator.alloc = tz_malloc;
  allocator.realloc = tz_realloc;
  allocator.dealloc = tz_free;
  allocator.user_data = NULL;

  return allocator;
}

////////////////////////////////////////////////////////////////////////////////
// TZ GFX
////////////////////////////////////////////////////////////////////////////////

void tz_create_pool(tz_pool* pool, size_t capacity, const tz_cb_allocator* allocator)
{
  pool->allocator = *allocator;
  pool->free_indices = (uint32_t*) TZ_ALLOC(pool->allocator, sizeof(uint32_t) * capacity, 4);
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

  tz_cb_allocator allocator;
  void* backend_data;
};

static void create_gfx_device(tz_gfx_device* device, const tz_gfx_device_params* device_config)
{
  device->allocator = device_config->allocator;
  
  tz_create_pool(&device->shader_stage_pool, device_config->max_shader_stages, &device_config->allocator);
  tz_create_pool(&device->shader_pool, device_config->max_shaders, &device_config->allocator);
  tz_create_pool(&device->buffer_pool, device_config->max_buffers, &device_config->allocator);
  tz_create_pool(&device->vertex_format_pool, device_config->max_vertex_formats, &device_config->allocator);
  tz_create_pool(&device->pipeline_pool, device_config->max_pipelines, &device_config->allocator);
}

static void destroy_gfx_device(tz_gfx_device* device)
{
  tz_delete_pool(&device->shader_stage_pool);
  tz_delete_pool(&device->shader_pool);
  tz_delete_pool(&device->buffer_pool);
  tz_delete_pool(&device->vertex_format_pool);
  tz_delete_pool(&device->pipeline_pool);
}

////////////////////////////////////////////////////////////////////////////////
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>

typedef struct 
{
  GLuint* shader_stages;
  GLuint* shader_programs;
  GLuint* buffers;
  tz_vertex_format_params* vertex_formats; 
  tz_pipeline_params* pipelines;

  tz_gfx_device_params params;
} tz_gfx_device_gl;

#define DEVICE_ALIGN 16

tz_gfx_device* tz_create_device(const tz_gfx_device_params* device_config)
{
  size_t total_size = sizeof(tz_gfx_device) + DEVICE_ALIGN
                    + sizeof(tz_gfx_device_gl) + DEVICE_ALIGN
                    + device_config->max_shader_stages * sizeof(GLuint) + DEVICE_ALIGN
                    + device_config->max_shaders * sizeof(GLuint) + DEVICE_ALIGN
                    + device_config->max_buffers * sizeof(GLuint) + DEVICE_ALIGN
                    + device_config->max_vertex_formats * sizeof(tz_vertex_format) + DEVICE_ALIGN
                    + device_config->max_pipelines * sizeof(tz_pipeline) + DEVICE_ALIGN;
  
  tz_gfx_device* return_device = (tz_gfx_device*) TZ_ALLOC(device_config->allocator, total_size, DEVICE_ALIGN);
  create_gfx_device(return_device, device_config);
  
  tz_gfx_device_gl* device = (tz_gfx_device_gl*) align_forward(return_device + 1, DEVICE_ALIGN);
  device->shader_stages = (GLuint*) align_forward(device + 1, DEVICE_ALIGN);
  device->shader_programs = (GLuint*) align_forward(device->shader_stages + device_config->max_shader_stages, DEVICE_ALIGN);
  device->buffers = (GLuint*) align_forward(device->shader_programs + device_config->max_shaders, DEVICE_ALIGN);
  device->vertex_formats = (tz_vertex_format_params*) align_forward(device->buffers + device_config->max_buffers, DEVICE_ALIGN);
  device->pipelines = (tz_pipeline_params*) align_forward(device->vertex_formats + device_config->max_vertex_formats, DEVICE_ALIGN);
  device->params = *device_config;

  return return_device;
}

void tz_delete_device(tz_gfx_device* device)
{
  destroy_gfx_device(device);
  TZ_FREE(device->allocator, device);
}

tz_shader_stage tz_create_shader_stage(tz_gfx_device* device, const tz_shader_stage_params* shader_stage_create_info)
{
  tz_gfx_device_gl* device_gl = (tz_gfx_device_gl*) device->backend_data;
}
