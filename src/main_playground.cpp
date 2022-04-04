// STL includes
#include <array>
#include <cstdlib>
#include <exception>
#include <map>
#include <string>
#include <vector>

// OpenGL includes
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

// Misc includes
#include <fmt/core.h>

// Core includes
#include <metameric/core/define.h>
#include <metameric/core/opengl.h>


/**
 * Preprocessor defines
 */

// Assert sugar
#define runtime_assert(expr, msg) detail::runtime_assert_(expr, msg, __FILE__, __LINE__);
#define runtime_gl_assert(msg) detail::runtime_gl_assert_(msg, __FILE__, __LINE__);


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

int main() {
  try {
    init_glfw();

    using namespace metameric;


    GLBuffer bf = { 0.5f, 0.5f, 0.5f }; // { 0.5, 0.5, 0.5, 0.5 };
    runtime_gl_assert("Buffer creation");

    std::vector<float> v({0.7f, 0.7f, 0.6f});
    bf.set(v);
    runtime_gl_assert("Buffer set");

    auto vf = bf.get_as<std::vector<float>>();
    runtime_gl_assert("Buffer get");
    
    
    // bf.set({ 0.66, 0.66, 0.66 });

    // print_container(f);
    print_container(vf);

    // fmt::print("{}\n", bf.size());


    // GLBuffer bvv(std::span{ v });

    // TestBuffer<vec2> b(16);
    // fmt::print("{}\n", b.size());
    // TestBuffer<float> b_ = TestBuffer<vec2>(16);
    // fmt::print("{}\n", b_.size());



    // std::vector<float> v(16, 2.5f);

    // TestBuffer<float> b(v);
    // fmt::print("{}", b.size());
    // b = TestBuffer<float>();
    // fmt::print("{}", b.size());

    /* std::vector<float> v(16, 2.5f);
    for (auto &f : v) {
      fmt::print("{} ", f);
    }
    fmt::print("\n");

    GLBuffer a(v.size() * sizeof(float), v.data());

    std::vector<float> v2 = a.get_data<float>();
    for (auto &f : v2) {
      fmt::print("{} ", f);
    }
    fmt::print("\n"); */

    dstr_glfw();
  } catch (const std::exception &e) {
    fmt::print(stderr, e.what());
    return EXIT_FAILURE;
  }
  return 0;
}