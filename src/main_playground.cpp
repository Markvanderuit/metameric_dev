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
#include <metameric/core/gl/buffer.h>
#include <metameric/core/gl/texture.h>
#include <metameric/core/gl/detail/exception.h>


/**
 * Globals
 */

GLFWwindow * glfw_handle;


/**
 * Assert and exception code
 */

namespace detail {
struct RuntimeException : public std::exception {
  std::map<std::string, std::string> logs;

  RuntimeException() { }
  RuntimeException(const std::string &msg)
  : logs({{"message", msg}}) { }

  const char * what() const noexcept override {
    std::string s = "Runtime exception\n";
    std::string fmt = "- {:<7} : {}\n";

    for (const auto &[key, log] : logs)
      s += fmt::format(fmt, key, log);

    return (_what = s).c_str();
  }
  
private:
  mutable std::string _what;
};
  
inline void runtime_gl_assert_(const std::string &msg, const char *file, int line) {
  GLenum err = glGetError();
  guard(err != GL_NO_ERROR);
  RuntimeException e(msg);
  e.logs["gl_err"] = std::to_string(err);
  e.logs["file"] = std::string(file);
  e.logs["line"] = std::to_string(line);
  throw e;
}

inline void runtime_assert_(bool expr, const std::string &msg, const char *file, int line) {
  guard(!expr);
  RuntimeException e(msg);
  e.logs["file"] = file;
  e.logs["line"] = std::to_string(line);
  throw e;
}
} // namespace detail


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

void test_eigen() {
  using namespace metameric;

  
  Vector3f v = { 1.5, 1.5, 1.5 };
  // v = v.array().pow(2.0f);
  v = eig::pow(v.array(), 2.0f);
  // v = eg::pow(2.0f, v);
  std::cout << v << std::endl;
  // fmt::print("{}", v);

  // using Eigen::MatrixXf;
  // using Eigen::VectorXf;

  // MatrixXf m = MatrixXf::Random(3, 3);
  // m = (m + MatrixXf::Constant(3, 3, 1.2f)) * 50.f;
  // std::cout << m << std::endl;

  // VectorXf v(3);
  // v << 1, 2, 3;
  // std::cout << v << std::endl;

  // std::cout << m * v << std::endl;
}

void render() {
  std::vector<float> vf(4);
  std::vector<int> vi(4);
  
  using namespace metameric;

  auto storage_flags = gl::BufferStorageFlags::eClient
                      | gl::BufferStorageFlags::eDynamic;
  auto mapping_flags = gl::BufferMappingFlags::eRead
                      | gl::BufferMappingFlags::eWrite
                      | gl::BufferMappingFlags::eCoherent
                      | gl::BufferMappingFlags::ePersistent;
                      
  gl::Buffer buffer(256 * 256 * sizeof(uint), nullptr, 0 | gl::BufferStorageFlags::eDynamic);
  buffer.fill({ 1u });
  buffer.set({4u}, 1, 0);
  auto v = buffer.get_as<std::vector<uint>>();
  gl_assert("After buffer creation");

  gl::Texture tex_0(gl::TextureFormat::eRGBA32Float, 1, 256, 256);
  VectorXi vx {{ 256, 256 }};
  gl::Texture tex_1(gl::TextureFormat::eR16Int, 1, Vector2i { 256, 256 });
  fmt::print("Hello!");
  gl_assert("After texture creation");

 /*  gl::Buffer buffer({4.f, 4.f, 4.f, 4.f}, storage_flags, mapping_flags);
  auto v = buffer.get_as<std::vector<float>>();
  
  buffer.set({3.f}, 1, 1);
  buffer.set({2.f, 1.f}, 2, 2);
  print_container(buffer.get(v));

  gl::Buffer other_buffer = buffer.copy();
  other_buffer.set({-1.f, -2.f}, 2, 2);

  buffer.fill({ 16.f });
  buffer.clear(2);

  print_container(buffer.get(v));
  print_container(other_buffer.get(v)); */

}

int main() {
  try {
    init_glfw();
    render();
    test_eigen();
    dstr_glfw();
  } catch (const std::exception &e) {
    fmt::print(stderr, e.what());
    return EXIT_FAILURE;
  }
  return 0;
}