// STL includes
#include <cstdlib>
#include <exception>
#include <map>
#include <string>

// OpenGL includes
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

// Imgui includes
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// Misc includes
#include <ImGuizmo.h>
#include <fmt/core.h>

// Core includes
#include <metameric/core/define.h>

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
int window_width, window_height;


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
  glfwWindowHint(GLFW_VISIBLE, 1);
  glfwWindowHint(GLFW_DECORATED, 1);
  glfwWindowHint(GLFW_FOCUSED, 1);
  glfwWindowHint(GLFW_RESIZABLE, 1);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);

  glfw_handle = glfwCreateWindow(1024, 768, "Imgui test", nullptr, nullptr);
  
  runtime_assert(glfw_handle, "glfwCreateWindow(...) failed");

  glfwMakeContextCurrent(glfw_handle);

  runtime_assert(gladLoadGL(), "gladLoadGL() failed");
}

void dstr_glfw() {
  glfwDestroyWindow(glfw_handle);
  glfwTerminate();
  glfw_handle = nullptr;
}

void init_imgui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui::StyleColorsDark();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  
  ImGui_ImplGlfw_InitForOpenGL(glfw_handle, true);
  ImGui_ImplOpenGL3_Init("#version 460");

  runtime_gl_assert("ImGui initialization");
}

void dstr_imgui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  
  runtime_gl_assert("ImGui destruction");
}

void begin_render_glfw() {
  glfwPollEvents();
}

void end_render_glfw() {
  glfwSwapBuffers(glfw_handle);
  glfwGetFramebufferSize(glfw_handle, &window_width, &window_height);
}

void begin_render_imgui() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void end_render_imgui() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


/**
 * Program code
 */

void render_loop() {
  while (!glfwWindowShouldClose(glfw_handle)) {
    begin_render_glfw();
    begin_render_imgui();
    runtime_gl_assert("Begin of render loop");

    // Clear framebuffer
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClearDepth(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Add some ImGui components
    ImGui::ShowDemoWindow();
    ImGui::Begin("Hello world");
    ImGui::Text("Uhh.....");
    ImGui::End();

    end_render_imgui();
    end_render_glfw();
    runtime_gl_assert("End of render loop");
  }
}

int main() {
  try { 
    init_glfw();
    init_imgui();

    using namespace metameric;
    // GLBuffer a(4 * sizeof(uint));

    // GLBuffer a;
    // GLBuffer b(4 * sizeof(uint));
    // fmt::print("a: {}, b: {}\n", a.handle(), b.handle());
    GLBuffer a = GLBuffer(4 * sizeof(uint));
    GLBuffer b = GLBuffer(4 * sizeof(uint));
    uint handle = a.handle();
    fmt::print("a {} {}\n", a.handle(), glIsBuffer(handle));
    a = GLBuffer();
    fmt::print("a {} {}\n", a.handle(), glIsBuffer(handle));
    
    fmt::print("init? {}\n", a.is_init());
    fmt::print("init? {}\n", b.is_init());
    fmt::print("Equality: {}\n", a == a);
    fmt::print("Equality: {}\n", a == b);
    
    // fmt::print("a: {}, b: {}\n", a.handle(), b.handle());

    // render_loop();

    dstr_imgui();
    dstr_glfw();
  } catch (const std::exception  e) {
    fmt::print(stderr, e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}