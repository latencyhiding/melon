#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char* load_text_file(const char* filename)
{
  FILE* f = fopen(filename, "r");
  size_t size = 0;
  char* data = NULL;

  if (!f)
  {
    TZ_LOG("Can't find file: %s\n", filename);
    return NULL;
  }

  fseek(f, 0L, SEEK_END);
  size = ftell(f);
  rewind(f);
 
  data = (char*) malloc(size + 1);
  memset(data, 0, size + 1);

  fread(data, 1, size, f);
  fclose(f);

  return data;
}

GLenum glCheckError()
{
  GLenum errorCode;
  while ((errorCode = glGetError()) != GL_NO_ERROR)
  {
    const char* error;
    switch (errorCode)
    {
    case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
    case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
    case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
    case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
    case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
    case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
    case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
    }
    printf("%s\n", error);
  }
  return errorCode;
}

#define WIDTH 800
#define HEIGHT 600

int main(int argc, char ** argv)
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

  const char* vertex_source = load_text_file("../assets/shaders/passthrough.vert");
  TZ_ASSERT(vertex_source);
  const char* fragment_source = load_text_file("../assets/shaders/passthrough.frag");
  TZ_ASSERT(fragment_source);

  tz_shader_params shader_params = (tz_shader_params) {
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

  free((void*) vertex_source);
  free((void*) fragment_source);

  float vertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f
  };

  tz_buffer_params buffer_params = (tz_buffer_params) {
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
        .size = 2,
        .divisor = 0
      }
    },
      .shader_program = shader_program
  };
  tz_pipeline pipeline = tz_create_pipeline(device, &pipeline_params);

  tz_draw_call_params draw_calls = (tz_draw_call_params) {
    .draw_type = TZ_TRIANGLES,
      .instances = 1,
      .base_vertex = 0,
      .num_vertices = 3
  };

  tz_draw_group draw_group = (tz_draw_group) {
    .pipeline = pipeline,
    .resources = {
      .buffers = {
        [0] = vertex_buffer
      }
    },
    .draw_calls = &draw_calls,
    .num_draw_calls = 1
  };

  glClearColor(0, 0, 0, 0);
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    tz_execute_draw_groups(device, &draw_group, 1);
    glCheckError();

    glfwSwapBuffers(window);
  }

  tz_delete_shader(device, shader_program);
  tz_delete_pipeline(device, pipeline);
  tz_delete_device(device);

  return 0;
}
