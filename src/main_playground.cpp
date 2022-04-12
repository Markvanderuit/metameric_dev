// STL includes
#include <array>
#include <cstdlib>
#include <exception>
#include <map>
#include <string>
#include <vector>
#include <iostream>

// OpenGL includes
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

// Misc includes
#include <fmt/core.h>

// Core includes
#include <metameric/core/define.h>
#include <metameric/core/math.h>
#include <metameric/core/exception.h>

// GL includes
#include <metameric/gl/detail/assert.h>
#include <metameric/gl/buffer.h>
#include <metameric/gl/sampler.h>
#include <metameric/gl/texture.h>

/**
 * Globals
 */

GLFWwindow * glfw_handle;


/**
 * Render loop setup/teardown functions
 */

void init_glfw() {
  runtime_assert(glfwInit(), "glfwInit() failed");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, 1);
  glfwWindowHint(GLFW_VISIBLE, 0);
  glfwWindowHint(GLFW_DECORATED, 0);
  glfwWindowHint(GLFW_FOCUSED, 0);
  glfwWindowHint(GLFW_RESIZABLE, 0);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);

  glfw_handle = glfwCreateWindow(1, 1, "", nullptr, nullptr);
  
  runtime_assert(glfw_handle, "glfwCreateWindow(...) failed");

  glfwMakeContextCurrent(glfw_handle);

  runtime_assert(gladLoadGL(), "gladLoadGL() failed");
}

void dstr_glfw() {
  glfwDestroyWindow(glfw_handle);
  glfwTerminate();
  glfw_handle = nullptr;
}

void begin_render_glfw() {
  glfwPollEvents();
}

void end_render_glfw() {
  glfwSwapBuffers(glfw_handle);
}


/**
 * Program code
 */

template <typename C>
void print_container(const C &c) {
  fmt::print("{{");
  for (auto f : c) {
    fmt::print(" {}", f);
  }
  fmt::print(" }}\n");
}

using uint = unsigned int;

void render() {
  using namespace metameric;

  gl::Sampler sampler = {{
    .min_filter   = gl::SamplerMinFilter::eNearest,
    .mag_filter   = gl::SamplerMagFilter::eLinear,
    .wrap         = gl::SamplerWrap::eRepeat
  }};

  std::vector<float> buffer = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  };
  std::vector<float> readback_buffer(16);
  // buffer[7] = 256; buffer[3] = 8192; buffer[15] = 314;

  gl::Texture1d1f texture = {{
    .dims = Array1i(16),
    .levels = 5,
    .data = buffer.data(),
    .data_size = buffer.size() * sizeof(float)
  }};
  
  gl_assert("texture init");

  /* gl::Texture1d1f texture = {
    Array1i(16),
    5,
    buffer.data(),
    buffer.size() * sizeof(float)
  }; */
  // gl_assert("texture init");

  // gl::Texture1d1ui texture(Array1i(16), 1, buffer.data(), buffer.size() * sizeof(uint));

  fmt::print("input:");
  print_container(buffer);

  // texture.set_image(buffer.data(), buffer.size() * sizeof(float));
  // gl_assert("texture set");

  texture.get_image(readback_buffer.data(), readback_buffer.size() * sizeof(float), 0);
  texture.get_subimage(readback_buffer.data(), readback_buffer.size() * sizeof(float), 1,
    Array1i(8), Array1i(0));
  gl_assert("texture get");
  
  fmt::print("get:");
  print_container(readback_buffer);

  texture.clear_image(nullptr, 0);
  texture.get_image(readback_buffer.data(), readback_buffer.size() * sizeof(float));
  
  fmt::print("clear:");
  print_container(readback_buffer);
}

int main() {
  try {
    init_glfw();
    render();
    dstr_glfw();
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return 0;
}