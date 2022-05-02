// STL includes
#include <array>
#include <cstdlib>
#include <exception>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <span>

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
#include <metameric/gl/draw.h>
#include <metameric/gl/framebuffer.h>
#include <metameric/gl/program.h>
#include <metameric/gl/sampler.h>
#include <metameric/gl/sync.h>
#include <metameric/gl/texture.h>
#include <metameric/gl/vertexarray.h>
#include <metameric/gl/utility.h>
#include <metameric/gl/window.h>

/* 
  Boilerplate
*/

template <typename C>
void print_container(const C &c) {
  fmt::print("{{");
  for (auto f : c) {
    fmt::print(" {}", f);
  }
  fmt::print(" }}\n");
}

template <typename T, typename O = std::byte>
std::span<O> reinterpret_span(std::span<T> s) {
  return { reinterpret_cast<O *>(s.data()), s.size_bytes() / sizeof(O) };
}


/* 
  Program objects
*/

metameric::gl::Window window;
metameric::gl::Framebuffer triangle_framebuffer;
metameric::gl::Buffer triangle_vertex_buffer;
metameric::gl::Buffer triangle_color_buffer;
metameric::gl::Buffer triangle_index_buffer;
metameric::gl::Vertexarray triangle_array;
metameric::gl::Program triangle_program;
metameric::gl::DrawInfo triangle_draw;

std::vector<float> clear_value = { 1.f, 0.f, 1.f, 0.f };
std::vector<metameric::Vector3f> triangle_vertices = {
  { 1.f, 1.f, 0.f },
  { -1.f, 1.f, 0.f },
  { 0.f, -1.f, 0.f },
};
std::vector<metameric::Vector3f> triangle_colors = {
  { 1.f, 0.f, 0.f },
  { 0.f, 1.f, 0.f },
  { 0.f, 0.f, 1.f }
};
std::vector<metameric::Vector3ui> triangle_indices = {
  { 0, 1, 2 }
};


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
  triangle_framebuffer = gl::Framebuffer::default_framebuffer();

  // Upload triangle data into buffer objects
  triangle_vertex_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_vertices)) });
  triangle_color_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_colors)) });
  triangle_index_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_indices)) });
                                       
  // Setup triangle vertex array object
  std::vector<gl::VertexBufferInfo> triangle_buffer_info = {
    { .buffer = &triangle_vertex_buffer, .binding = 0, .stride = sizeof(Vector3f) },
    { .buffer = &triangle_color_buffer, .binding = 1, .stride = sizeof(Vector3f) }};
  std::vector<gl::VertexAttribInfo> triangle_attrib_info = {
    { .attrib_binding = 0, .buffer_binding = 0, 
      .format_type = gl::VertexFormatType::eFloat, 
      .format_size = gl::VertexFormatSize::e3 },
    { .attrib_binding = 1, .buffer_binding = 1,
      .format_type = gl::VertexFormatType::eFloat,
      .format_size = gl::VertexFormatSize::e3 }};
  triangle_array = gl::Vertexarray({ .buffers = triangle_buffer_info,
                                     .attribs = triangle_attrib_info,
                                     .elements = &triangle_index_buffer });
                                     
  // Setup draw program
  triangle_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                    .file_path = "../resources/shaders/triangle.vert.spv" },
                                  { .type = gl::ShaderType::eFragment, 
                                    .file_path = "../resources/shaders/triangle.frag.spv" }});
  triangle_program.uniform("scalar", 0.5f);


  // Setup data fro draw call of vertex array object
  triangle_draw = { .type = gl::PrimitiveType::eTriangles,
                    .array = &triangle_array,
                    .vertex_count = (uint) triangle_indices.size() * 3,
                    .program = &triangle_program };

  // Bind objects used constantly
  triangle_framebuffer.bind();
}

void step() {
  using namespace metameric;

  // Specify draw capabilities for this scope
  std::vector<gl::state::ScopedSet> capabilities = {{ gl::DrawCapability::eCullFace, true },
                                                    { gl::DrawCapability::eDepthTest, true },
                                                    { gl::DrawCapability::eBlendOp, false }};

  // Prepare for draw call
  gl::state::set_viewport(window.framebuffer_size());
  triangle_framebuffer.clear<Array3f>(gl::FramebufferAttachmentType::eColor, { 1, 0, 1 });

  // Submit draw call
  gl::draw(triangle_draw);
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