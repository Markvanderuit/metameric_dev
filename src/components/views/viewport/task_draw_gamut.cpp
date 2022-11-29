#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_gamut.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  /* constexpr std::array<uint, 8> gamut_elements = {
    0, 1, 2, 0,
    3, 1, 3, 2
  }; */
  
  constexpr std::array<uint, 12> gamut_elements = {
    0, 1, 2, 
    1, 3, 2,
    3, 0, 2,
    3, 1, 0
  };

  ViewportDrawGamutTask::ViewportDrawGamutTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get externally shared resources 
    auto &e_gamut_buffer   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_colr");

    // Define flags for creation of a persistent, write-only flushable buffer map
    auto create_flags = gl::BufferCreateFlags::eMapWrite 
                      | gl::BufferCreateFlags::eMapPersistent;
    auto map_flags    = gl::BufferAccessFlags::eMapWrite 
                      | gl::BufferAccessFlags::eMapPersistent
                      | gl::BufferAccessFlags::eMapFlush;
                      
    // Setup objects for gamut line draw
    m_gamut_elem_buffer = gl::Buffer({ .data = cnt_span<const std::byte>(gamut_elements) });
    m_gamut_array = gl::Array({
      .buffers = {{ .buffer = &e_gamut_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_gamut_elem_buffer
    });
    m_gamut_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                     .path = "resources/shaders/viewport/draw_color_array.vert" },
                                   { .type = gl::ShaderType::eFragment,  
                                     .path = "resources/shaders/viewport/draw_color_uniform_offset.frag" }});
    m_gamut_draw = { .type             = gl::PrimitiveType::eTriangles,
                     .vertex_count     = (uint) gamut_elements.size(),
                     .bindable_array   = &m_gamut_array,
                     .bindable_program = &m_gamut_program };

    // Set non-changing uniform values
    m_gamut_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
    m_gamut_program.uniform("u_alpha",        1.f);
    m_gamut_program.uniform("u_offset",       .5f);
  }

  void ViewportDrawGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
                                
    // Get shared resources 
    auto &e_viewport_arcball   = info.get_resource<detail::Arcball>("viewport_input", "arcball");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true) };
    
    // Update program uniforms
    m_gamut_program.uniform("u_camera_matrix", e_viewport_arcball.full().matrix());

    // Dispatch draws for gamut shape
    gl::state::set_op(gl::DrawOp::eLine);
    gl::dispatch_draw(m_gamut_draw);
    gl::state::set_op(gl::DrawOp::eFill);
  }
} // namespace met