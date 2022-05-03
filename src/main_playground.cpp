// STL includes
#include <cstdlib>
#include <vector>
#include <span>

// Misc includes
#include <fmt/core.h>

// Metameric includes
#include <metameric/core/define.h>
#include <metameric/core/math.h>
#include <metameric/core/exception.h>

// GL includes
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>

/* 
  Program objects
*/

gl::Window window;
gl::Framebuffer framebuffer;
gl::Buffer vertex_buffer, color_buffer, index_buffer;
gl::Array array;
gl::Program program;
gl::DrawInfo main_draw;

metameric::Vector4f framebuffer_clear = { 0.f, 0.f, 0.f, 0.f };
std::vector<metameric::Vector3f> triangle_vertices = {{ 1.f, 1.f, 0.f },
                                                      { -1.f, 1.f, 0.f },
                                                      { 0.f, -1.f, 0.f }};
std::vector<metameric::Vector3f> triangle_colors = {{ 1.f, 0.f, 0.f },
                                                    { 0.f, 1.f, 0.f },
                                                    { 0.f, 0.f, 1.f }};
std::vector<metameric::Vector3ui> triangle_indices = {{ 0, 1, 2 }};


/**
 * Program code
 */

void init() {
  using namespace metameric;
    
  // Setup OpenGL context and primary window
  gl::WindowFlags flags = gl::WindowFlags::eVisible   | gl::WindowFlags::eDecorated
                        | gl::WindowFlags::eSRGB      | gl::WindowFlags::eFocused
                        | gl::WindowFlags::eResizable | gl::WindowFlags::eDebug;
  window = gl::Window({ .size = { 512, 512 }, .title = "Hello window", .flags = flags });

  // Setup framebuffer as default
  framebuffer = gl::Framebuffer::make_default();

  // Upload triangle data into buffer objects
  vertex_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_vertices)) });
  color_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_colors)) });
  index_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_indices)) });
                                       
  // Setup triangle vertex array object
  std::vector<gl::VertexBufferInfo> triangle_buffer_info = {
    { .buffer = &vertex_buffer, .binding = 0, .stride = sizeof(Vector3f) },
    { .buffer = &color_buffer, .binding = 1, .stride = sizeof(Vector3f) }};
  std::vector<gl::VertexAttributeInfo> triangle_attrib_info = {
    { .attribute_binding = 0, .buffer_binding = 0, 
      .format_type = gl::VertexFormatType::eFloat, 
      .format_size = gl::VertexFormatSize::e3 },
    { .attribute_binding = 1, .buffer_binding = 1,
      .format_type = gl::VertexFormatType::eFloat,
      .format_size = gl::VertexFormatSize::e3 }};
  array = gl::Array({ .buffers = triangle_buffer_info,
                      .attributes = triangle_attrib_info,
                      .elements = &index_buffer });
                                     
  // Setup draw program
  program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                           .path = "../resources/shaders/triangle.vert.spv" },
                         { .type = gl::ShaderType::eFragment, 
                           .path = "../resources/shaders/triangle.frag.spv" }});
  program.uniform("scalar", 0.5f);


  // Setup data fro draw call of vertex array object
  main_draw = { .type = gl::PrimitiveType::eTriangles,
                .array = &array,
                .vertex_count = (uint) triangle_indices.size() * 3  };

  // Bind objects used constantly
  framebuffer.bind();
  program.bind();
}

void step() {
  using namespace metameric;

  // Specify draw capabilities for this scope
  std::vector<gl::state::ScopedSet> capabilities = {{ gl::DrawCapability::eCullFace, true },
                                                    { gl::DrawCapability::eDepthTest, true },
                                                    { gl::DrawCapability::eBlendOp, false }};

  // Prepare for draw call
  gl::state::set_viewport(window.framebuffer_size());
  framebuffer.clear<Vector4f>(gl::FramebufferType::eColor, framebuffer_clear);

  // Submit draw call
  gl::dispatch(main_draw);
}

void run() {
  while (!window.should_close()) {
    window.poll_events();
    step();
    window.swap_buffers();
  }
}

int main() {
  try {
    init();
    run();
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return 0;
}