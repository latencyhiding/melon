#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>

#include <engine/tz_graphics_backend.h>
#include <engine/tz_error.h>

static void error_callback(int error, const char *description)
{
  fprintf(stderr, "%s", description);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);
}

static const char* load_text_file(const tz_cb_allocator* allocator, const char* filename)
{
  FILE* f = fopen(filename, "r");
  size_t size = 0;
  char* data = NULL;
  const tz_cb_allocator* alloc = allocator ? allocator : tz_default_cb_allocator();

  if (!f)
  {
    TZ_LOG("Can't find file: %s\n", filename);
    return NULL;
  }

  fseek(f, 0L, SEEK_END);
  size = ftell(f);
  rewind(f);
 
  data = (char*)malloc(size + 1);

  fread(data, 1, size, f);
  fclose(f);

  data[size] = '\0';

  return data;
}

#define WIDTH 800
#define HEIGHT 600

int main()
{
  GLFWwindow* window;
  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwSetErrorCallback(error_callback);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "", NULL, NULL);
  if (!window)
  {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);

  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glfwSetKeyCallback(window, key_callback);

  tz_gfx_device* device = tz_create_device(NULL);

  const char* vertex_source = load_text_file(tz_default_cb_allocator(), "../assets/shaders/passthrough.vert");
  TZ_ASSERT(vertex_source);
  const char* fragment_source = load_text_file(tz_default_cb_allocator(), "../assets/shaders/passthrough.frag");
  TZ_ASSERT(fragment_source);

  tz_shader_params shader_params = tz_gen_shader_params();
  shader_params = (tz_shader_params) {
    .vertex_shader = {
      .name = "passthrough.vert",
      .source = vertex_source,
      .size = strlen(vertex_source)
    },
    .fragment_shader = {
      .name = "passthrough.frag",
      .source = fragment_source,
      .size = strlen(fragment_source)
    }
  };

  tz_shader shader_program = tz_create_shader(device, &shader_params);

  free(vertex_source);
  free(fragment_source);

  float vertices[] = {
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f
  };

  tz_buffer_params buffer_params = tz_gen_buffer_params();
  buffer_params = (tz_buffer_params) {
    .data = vertices,
    .size = sizeof(vertices),
    .usage = TZ_STATIC_BUFFER
  };

  tz_buffer vertex_buffer = tz_create_buffer(device, &buffer_params);

  tz_pipeline_params pipeline_params = (tz_pipeline_params) {
    .vertex_attribs = {
      [0] = {
        .name = "position",
        .buffer_binding = 0,
        .offset = 0,
        .type = TZ_FORMAT_FLOAT,
        .size = 3,
        .divisor = 0
      }
    }
  };

  tz_pipeline pipeline = tz_create_pipeline(device, &pipeline_params);

  tz_draw_resources resources = (tz_draw_resources) {
    .buffers = {
      [0] = vertex_buffer
    }
  };

  tz_draw_call_params draw_call_params = (tz_draw_call_params) {
    .draw_type = TZ_TRIANGLES,
      .instances = 1,
      .base_vertex = 0,
      .num_vertices = 3
  };

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    glClearColor(0, 0, 0, 0);

    tz_execute_draw_call(device, pipeline, &resources, &draw_call_params);

    glfwSwapBuffers(window);
  }

  tz_delete_shader(device, shader_program);
  tz_delete_pipeline(device, pipeline);
  tz_delete_device(device);

  return 0;
}
