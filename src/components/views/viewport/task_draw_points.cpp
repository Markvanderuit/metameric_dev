#include <metameric/core/detail/trace.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/viewport/task_draw_points.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <execution>

namespace met {
  ViewportDrawPointsTask::ViewportDrawPointsTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawPointsTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get externally shared resources
    // auto &e_point_buffesr = info.get_resource<gl::Buffer>("gen_ocs", "color_buffer");

    // Construct buffer object and draw components
    m_program = {{ .type = gl::ShaderType::eVertex, 
                   .path = "resources/shaders/viewport/draw_color_array.vert" },
                 { .type = gl::ShaderType::eFragment,  
                   .path = "resources/shaders/viewport/draw_color.frag" }};
    /* m_array = {{
      .buffers = {{ .buffer = &e_point_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    }}; */
    /* m_draw = { .type             = gl::PrimitiveType::ePoints,
               .vertex_count     = (uint) (e_point_buffer.size() / sizeof(eig::AlArray3f)),
               .bindable_array   = &m_array,
               .bindable_program = &m_program }; */

    // Set non-changing uniform values
    m_program.uniform("u_model_matrix",  eig::Matrix4f::Identity().eval());
    
    m_stale = true;
  }

  void ViewportDrawPointsTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    if (m_stale) {
      if (info.has_resource("gen_ocs", "ocs_verts")) {
        auto &e_verts_buffer = info.get_resource<gl::Buffer>("gen_ocs", "ocs_verts");
        auto &e_elems_buffer = info.get_resource<gl::Buffer>("gen_ocs", "ocs_elems");
        m_array = {{
          .buffers  = {{ .buffer = &e_verts_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
          .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
          .elements = &e_elems_buffer
        }};
        m_draw = { .type             = gl::PrimitiveType::eTriangles,
                   .vertex_count     = (uint) (3 * e_elems_buffer.size() / sizeof(eig::Array3u)), // 3 * 64, // (uint) (e_elems_buffer.size() / sizeof()),
                   .bindable_array   = &m_array,
                   .bindable_program = &m_program };
        m_stale = false;
      } else {
        return;
      }
    }
    
    // Insert temporary window to modify draw settings
    if (ImGui::Begin("Point draw settings")) {
      ImGui::SliderFloat("Point size", &m_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();

    // Get externally shared resources 
    auto &e_frame_buffer = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer_msaa");
    auto &e_draw_texture = info.get_resource<gl::Texture2d3f>("viewport", "draw_texture");
    auto &e_arcball      = info.get_resource<detail::Arcball>("viewport", "arcball");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true) };

    // Prepare framebuffer as draw target
    e_frame_buffer.bind();
    gl::state::set_viewport(e_draw_texture.size());
    
    // Update program uniforms  
    m_program.uniform("u_camera_matrix", e_arcball.full().matrix());    

    // Dispatch draw call
    gl::state::set_point_size(m_psize);
    gl::dispatch_draw(m_draw);
  }
} // namespace met