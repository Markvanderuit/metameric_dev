#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_gamut.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite 
                                     | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite 
                                     | gl::BufferAccessFlags::eMapPersistent
                                     | gl::BufferAccessFlags::eMapFlush;

  /* constexpr std::array<uint, 12> gamut_elements = {
    0, 1, 2, 
    1, 3, 2,
    3, 0, 2,
    3, 1, 0
  }; */

  ViewportDrawGamutTask::ViewportDrawGamutTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get shared resources
    auto &e_app_data    = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data   = e_app_data.project_data;
    auto &e_gamut_elem = e_proj_data.gamut_elems;
    auto &e_gamut_vert = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_colr");

    // Define flags for creation of a persistent, write-only flushable buffer map
    auto create_flags = gl::BufferCreateFlags::eMapWrite 
                      | gl::BufferCreateFlags::eMapPersistent;
    auto map_flags    = gl::BufferAccessFlags::eMapWrite 
                      | gl::BufferAccessFlags::eMapPersistent
                      | gl::BufferAccessFlags::eMapFlush;
                      
    // Setup objects for gamut line draw
    m_gamut_elem_buffer  = {{ .data = cnt_span<const std::byte>(e_gamut_elem), .flags = buffer_create_flags }};
    m_gamut_elem_mapping = cast_span<eig::Array3u>(m_gamut_elem_buffer.map(buffer_access_flags));
    m_gamut_array = gl::Array({
      .buffers = {{ .buffer = &e_gamut_vert, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_gamut_elem_buffer
    });
    m_gamut_program = gl::Program({{ .type = gl::ShaderType::eVertex, 
                                     .path = "resources/shaders/viewport/draw_color_array.vert" },
                                   { .type = gl::ShaderType::eFragment,  
                                     .path = "resources/shaders/viewport/draw_color_uniform_offset.frag" }});
    m_gamut_draw = { .type             = gl::PrimitiveType::eTriangles,
                     .vertex_count     = static_cast<uint>(e_gamut_elem.size()) * 3,
                     .bindable_array   = &m_gamut_array,
                     .bindable_program = &m_gamut_program };
    m_gamut_draw_selection = { .type             = gl::PrimitiveType::eTriangles,
                               .vertex_count     = static_cast<uint>(e_gamut_elem.size()) * 3,
                               .bindable_array   = &m_gamut_array,
                               .bindable_program = &m_gamut_program };

    // Set non-changing uniform values
    m_gamut_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
    m_gamut_program.uniform("u_alpha",        1.f);
    m_gamut_program.uniform("u_offset",       .5f);

    m_gamut_vert_cache = e_gamut_vert.object();
    m_gamut_elem_count = static_cast<uint>(e_gamut_elem.size());
  }

  void ViewportDrawGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
                                
    // Get shared resources 
    auto &e_viewport_arcball = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_state_elems      = info.get_resource<std::vector<CacheState>>("project_state", "gamut_elems");
    auto &e_app_data         = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data        = e_app_data.project_data;
    auto &e_gamut_elem       = e_proj_data.gamut_elems;
    auto &e_gamut_vert       = info.get_resource<gl::Buffer>("gen_spectral_gamut", "buffer_colr");
    auto &e_elem_selection   = info.get_resource<std::vector<uint>>("viewport_input", "gamut_elem_selection");

    // Update array object in case gamut buffer was resized
    if (m_gamut_vert_cache != e_gamut_vert.object()) {
      m_gamut_vert_cache = e_gamut_vert.object();
      m_gamut_array.attach_buffer({{ .buffer = &e_gamut_vert, .index = 0, .stride = sizeof(eig::AlArray3f) }});
    }

    // Re-create gamut element buffer in case of nr. of vertices changes
    if (m_gamut_elem_count != static_cast<uint>(e_gamut_elem.size())) {
      m_gamut_array.detach_elements();

      if (m_gamut_elem_buffer.is_init() && m_gamut_elem_buffer.is_mapped()) 
        m_gamut_elem_buffer.unmap();
      m_gamut_elem_buffer  = {{ .data = cnt_span<const std::byte>(e_gamut_elem), .flags = buffer_create_flags }};
      m_gamut_elem_mapping = cast_span<eig::Array3u>(m_gamut_elem_buffer.map(buffer_access_flags));
      
      m_gamut_array.attach_elements(m_gamut_elem_buffer);
      m_gamut_draw.vertex_count = static_cast<uint>(e_gamut_elem.size()) * 3;
      m_gamut_elem_count = static_cast<uint>(e_gamut_elem.size());
    }

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true) };
    
    // Update program uniforms
    m_gamut_program.uniform("u_camera_matrix", e_viewport_arcball.full().matrix());

    // Dispatch draws for gamut lines
    gl::state::set_op(gl::DrawOp::eLine);
    gl::dispatch_draw(m_gamut_draw);
    gl::state::set_op(gl::DrawOp::eFill);

    // Dispatch draws for gamut selection
    if (e_elem_selection.size() > 0 ) {
      m_gamut_draw_selection.vertex_count = 3;
      m_gamut_draw_selection.vertex_first = e_elem_selection[0] * 3;
      gl::dispatch_draw(m_gamut_draw_selection);
    }
  }
} // namespace met