#include "graphics_backend.h"

#include <stdint.h>
#include <stdlib.h>

inline static void* align_forward(void* ptr, size_t align)
{
  uintptr_t uint_ptr = (uintptr_t) ptr;
  return (void*) (uint_ptr + (align - (uint_ptr % align)));
}

static void* tz_malloc(void* user_data, size_t size, size_t align)
{
  void* ptr = malloc(size + align);

  return ptr ? (void*) align_forward(ptr, align) : NULL;
}

static void* tz_realloc(void* user_data, void* ptr, size_t size, size_t align)
{
  void* new_ptr = realloc(ptr, size + align);

  return new_ptr ? (void*) align_forward(new_ptr, align) : NULL;
}

static void tz_free(void* user_data, void* ptr)
{
  free(ptr);
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
// OPENGL
////////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct tz_gfx_device
{
  GLuint* shader_stages;
  size_t shader_stages_size;
  size_t shader_stages_capacity;

  GLuint* shader_programs;
  size_t shader_programs_size;
  size_t shader_programs_capacity;

  GLuint* buffers;
  size_t buffers_size;
  size_t buffers_capacity;

  tz_vertex_format* vertex_formats; 
  size_t vertex_formats_size;
  size_t vertex_formats_capacity;

  tz_pipeline* pipelines;
  size_t pipelines_size;
  size_t pipelines_capacity;

  tz_cb_allocator allocator;
};

#define DEVICE_ALIGN 16

tz_gfx_device* tz_create_device(const tz_gfx_device_config* device_config)
{
  size_t total_size = sizeof(tz_gfx_device)
                    + device_config->max_shader_stages * sizeof(GLuint)
                    + device_config->max_shaders * sizeof(GLuint)
                    + device_config->max_buffers * sizeof(GLuint)
                    + device_config->max_vertex_formats * sizeof(tz_vertex_format)
                    + device_config->max_pipelines * sizeof(tz_pipeline);

  tz_gfx_device* device = (tz_gfx_device*) TZ_ALLOC(device_config->allocator, total_size, DEVICE_ALIGN);
  
  device->shader_stages = (GLuint*) align_forward(device + 1, DEVICE_ALIGN);
  device->shader_stages_size = 0;
  device->shader_stages_capacity = device_config->max_shader_stages;
  
  device->shader_programs = (GLuint*) align_forward(device->shader_stages + device->shader_stages_capacity, DEVICE_ALIGN);
  device->shader_programs_size = 0;
  device->shader_programs_capacity = device_config->max_shaders;

  device->buffers = (GLuint*) align_forward(device->shader_programs + device->shader_programs_capacity, DEVICE_ALIGN);
  device->buffers_size = 0;
  device->buffers_capacity = device_config->max_buffers;

  device->vertex_formats = (tz_vertex_format*) align_forward(device->buffers + device->buffers_capacity, DEVICE_ALIGN);
  device->vertex_formats_size = 0;
  device->vertex_formats_capacity = device_config->max_vertex_formats;

  device->pipelines = (tz_pipeline*) align_forward(device->vertex_formats + device->vertex_formats_capacity, DEVICE_ALIGN);
  device->pipelines_size = 0;
  device->pipelines_capacity = device_config->max_pipelines;

  return (tz_gfx_device*) device;
}

tz_shader_stage_id tz_create_shader_stage(tz_gfx_device* device, const tz_shader_stage* shader_stage_create_info)
{
    
}
