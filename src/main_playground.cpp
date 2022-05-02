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
#include <metameric/gl/window.h>

/* 
  Boilerplate
*/

std::vector<char> load_shader_binary(const std::string &file_path) {
  using namespace metameric;

  std::ifstream ifs(file_path, std::ios::ate | std::ios::binary);
  runtime_assert(ifs.is_open(),
    fmt::format("load_shader_binary(...), failed to load file \"{}\"", file_path));

  size_t file_size = static_cast<size_t>(ifs.tellg());
  std::vector<char> buffer(file_size);
  ifs.seekg(0);
  ifs.read((char *) buffer.data(), file_size);
  ifs.close();
  
  return buffer;
}

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
  window = gl::Window({ .size = { 512, 512 }, .title = "Hello window 1", .flags = flags });

  // Setup framebuffer as default
  triangle_framebuffer = gl::Framebuffer::default_framebuffer();

  // Upload triangle data into buffer objects
  triangle_vertex_buffer = gl::Buffer({ .size = triangle_vertices.size() * sizeof(Vector3f),
                                        .data = reinterpret_span<Vector3f>(triangle_vertices) });
  triangle_color_buffer = gl::Buffer({ .size = triangle_colors.size() * sizeof(Vector3f),
                                        .data = reinterpret_span<Vector3f>(triangle_colors) });
  triangle_index_buffer = gl::Buffer({ .size = triangle_indices.size() * sizeof(Vector3i),
                                       .data = reinterpret_span<Vector3ui>(triangle_indices),
                                       .flags = gl::BufferStorageFlags::eStorageDynamic });
                                       
  // Setup triangle vertex array object
  std::vector<gl::VertexBufferInfo> triangle_buffer_info = {
    { .buffer = triangle_vertex_buffer, .binding = 0, .stride = sizeof(Vector3f) },
    { .buffer = triangle_color_buffer, .binding = 1, .stride = sizeof(Vector3f) }};
  std::vector<gl::VertexAttribInfo> triangle_attrib_info = {
    { .attrib_binding = 0, .buffer_binding = 0, 
      .format_type = gl::VertexFormatType::eFloat, 
      .format_size = gl::VertexFormatSize::e3 },
    { .attrib_binding = 1, .buffer_binding = 1,
      .format_type = gl::VertexFormatType::eFloat,
      .format_size = gl::VertexFormatSize::e3 }};
  triangle_array = gl::Vertexarray({ .buffers = triangle_buffer_info,
                                     .attribs = triangle_attrib_info,
                                     .elements = triangle_index_buffer });

  // Setup data fro draw call of vertex array object
  triangle_draw = { .type = gl::PrimitiveType::eTriangles,
                    .array = &triangle_array,
                    .vertex_count = (uint) triangle_index_buffer.size() / sizeof(uint) };
                                     
  // Setup draw program
  triangle_program = gl::Program({
    { .type = gl::ShaderType::eVertex, 
      .data = load_shader_binary("../resources/shaders/triangle.vert.spv") },
    { .type = gl::ShaderType::eFragment, 
      .data = load_shader_binary("../resources/shaders/triangle.frag.spv") }});
  triangle_program.uniform("scalar", 0.5f);

  // Bind objects used constantly
  triangle_program.bind();
  triangle_framebuffer.bind();
}

void loop_step() {
  using namespace metameric;

  // Specify draw capabilities for this scope
  std::vector<gl::state::scoped_set> capabilities = {
    { gl::DrawCapability::eCullFace, true },
    { gl::DrawCapability::eDepthTest, true },
    { gl::DrawCapability::eBlendOp, false }
  };

  // Prepare for draw call
  gl::state::set_viewport(window.framebuffer_size());
  triangle_framebuffer.clear(gl::FramebufferClearType::eColor, 
                             reinterpret_span<float>(clear_value) );

  // Submit draw call
  gl::draw(triangle_draw);
}

void run_loop() {
  while (!window.should_close()) {
    loop_step();

    window.poll_events();
    window.swap_buffers();
  }
}

int main() {
  try {
    init();
    run_loop();
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return 0;
}