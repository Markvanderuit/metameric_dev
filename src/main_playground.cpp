// STL includes
#include <cstdlib>
#include <vector>
#include <span>

// Misc includes
#include <fmt/core.h>

// TBB includes
// #include <tbb/parallel_for.h>
// #include <tbb/tbb.h>
// #include <tbb/flow_graph.h>

// Metameric includes
#include <metameric/core/define.h>
#include <metameric/core/math.h>
#include <metameric/core/exception.h>

// GL includes
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/exception.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>
#include <small_gl/sampler.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <small_gl/window.hpp>

/* 
  Program objects
*/

gl::WindowFlags flags = gl::WindowFlags::eVisible   | gl::WindowFlags::eDecorated
                      | gl::WindowFlags::eSRGB      | gl::WindowFlags::eFocused
                      | gl::WindowFlags::eResizable | gl::WindowFlags::eDebug;

// Shared objects
gl::Buffer vertex_buffer, color_buffer, index_buffer;
gl::Program program;

// Per-thread objects
gl::Window primary_window, secondary_window;
gl::Framebuffer primary_framebuffer, secondary_framebuffer;
gl::Array primary_array, secondary_array;
gl::DrawInfo primary_draw, secondary_draw;

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

void init_windows() {
  using namespace metameric;

  primary_window = gl::Window({ .size = { 512, 512 }, 
                                .title = "Primary window",
                                .flags = flags });

  secondary_window = gl::Window({ .size = { 512, 512 },
                                  .title = "Secondary window",
                                  .is_main_context = false,
                                  .shared_context = &primary_window,
                                  .flags = flags });
}

void init_shared() {
  using namespace metameric;
  
  primary_window.attach_context();

  // Upload shader data into program object
  program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                           .path = "../resources/shaders/triangle.vert.spv" },
                         { .type = gl::ShaderType::eFragment, 
                           .path = "../resources/shaders/triangle.frag.spv" }});

  // Upload triangle data into buffer objects
  vertex_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_vertices)) });
  color_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_colors)) });
  index_buffer = gl::Buffer({ .data = std::as_bytes(std::span(triangle_indices)) });
}

void init_primary() {
  using namespace metameric;

  primary_window.attach_context();
                                                      
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
  primary_array = gl::Array({ .buffers = triangle_buffer_info,
                              .attributes = triangle_attrib_info,
                              .elements = &index_buffer });

  // Setup draw call object
  primary_draw = { .type = gl::PrimitiveType::eTriangles,
                   .array = &primary_array,
                   .vertex_count = (uint) triangle_indices.size() * 3  };

  // Setup framebuffer as default
  primary_framebuffer = gl::Framebuffer::make_default();
}

void init_secondary() {
  using namespace metameric;

  secondary_window.attach_context();
  
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
  secondary_array = gl::Array({ .buffers = triangle_buffer_info,
                                .attributes = triangle_attrib_info,
                                .elements = &index_buffer });

  // Setup draw call object
  secondary_draw = { .type = gl::PrimitiveType::eTriangles,
                     .array = &secondary_array,
                     .vertex_count = (uint) triangle_indices.size() * 3  };

  // Setup framebuffer as default
  secondary_framebuffer = gl::Framebuffer::make_default();
}

void step(gl::Window &window, gl::Framebuffer &framebuffer, gl::Program &program, gl::DrawInfo &draw, float scalar) {
  using namespace metameric;

  // Ensure window is active on this thread
  window.attach_context();
  window.poll_events();

  // Setup framebuffer
  gl::state::set_viewport(window.framebuffer_size());
  framebuffer.bind();
  framebuffer.clear<Vector4f>(gl::FramebufferType::eColor, framebuffer_clear);
  
  // Specify draw capabilities for this scope
  std::vector<gl::state::ScopedSet> capabilities = {{ gl::DrawCapability::eCullFace, true },
                                                    { gl::DrawCapability::eDepthTest, true },
                                                    { gl::DrawCapability::eBlendOp, false }};

  // Bind and configure this thread's program
  program.bind();
  program.uniform("scalar", scalar);
  
  // Submit draw call
  gl::dispatch(draw);

  // Finally, swap framebuffers
  window.swap_buffers();

  gl::gl_check();
}

void run() {
  bool should_run = true;
  while (should_run) {
    should_run = !primary_window.should_close()
              || !secondary_window.should_close();
    
    if (primary_window.is_init()) {
      if (primary_window.should_close()) {
        break;
      } else {
        step(primary_window, primary_framebuffer, program, primary_draw, 0.8f);
      }
    }

    if (secondary_window.is_init()) {
      if (secondary_window.should_close()) {
        secondary_window = { }; // destroy window
      } else {
        step(secondary_window, secondary_framebuffer, program, secondary_draw, 0.5f);
      }
    }
  }
}

int main() {
  try {
    init_windows();
    init_shared();
    init_primary();
    init_secondary();
    run();
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}