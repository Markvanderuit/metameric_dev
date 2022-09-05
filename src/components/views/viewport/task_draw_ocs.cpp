#include <metameric/core/detail/trace.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/viewport/task_draw_ocs.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <execution>

namespace met {
  ViewportDrawOCSTask::ViewportDrawOCSTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();
    
    // Get externally shared resources
    auto &e_point_buffer = info.get_resource<gl::Buffer>("gen_ocs", "color_buffer");

    // Construct draw components
    m_program = {{ .type = gl::ShaderType::eVertex, 
                   .path = "resources/shaders/viewport/draw_color_array.vert" },
                 { .type = gl::ShaderType::eFragment,  
                   .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};
    m_array_points = {{
      .buffers = {{ .buffer = &e_point_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
    }};
    m_draw_points = { .type           = gl::PrimitiveType::ePoints,
                      .vertex_count   = (uint) (e_point_buffer.size() / sizeof(eig::AlArray3f)),
                      .bindable_array = &m_array_points };

    // Set non-changing uniform values
    m_program.uniform("u_model_matrix",  eig::Matrix4f::Identity().eval());
    m_program.uniform("u_alpha", 0.33f);

    m_stale = true;
    m_stale_metset = true;
  }

  void ViewportDrawOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    if (m_stale && info.has_resource("gen_ocs", "ocs_verts")) {
      // Get shared resources
      auto &e_verts_buffer = info.get_resource<gl::Buffer>("gen_ocs", "ocs_verts");
      auto &e_elems_buffer = info.get_resource<gl::Buffer>("gen_ocs", "ocs_elems");
      
      // Create vertex array and draw command
      m_array_hull = {{
        .buffers  = {{ .buffer = &e_verts_buffer, .index = 0, .stride = sizeof(eig::AlArray3f) }},
        .attribs  = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &e_elems_buffer
      }};
      m_draw_hull = { .type           = gl::PrimitiveType::eTriangles,
                      .vertex_count   = (uint) (e_elems_buffer.size() / sizeof(uint)),
                      .bindable_array = &m_array_hull };
                      
      m_stale = false;
    }

    if (m_stale_metset && info.has_resource("gen_ocs", "metset_verts")) {
        auto &e_verts_buffer = info.get_resource<gl::Buffer>("gen_ocs", "metset_verts");
        auto &e_elems_buffer = info.get_resource<gl::Buffer>("gen_ocs", "metset_elems");
       
      // Create vertex array and draw command
      m_array_metset = {{
        .buffers = {{ .buffer = &e_verts_buffer, .index = 0, .stride = sizeof(AlColr) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &e_elems_buffer
      }};
      m_draw_metset = { .type = gl::PrimitiveType::eTriangles,
                        .vertex_count = (uint) (e_elems_buffer.size() / sizeof(uint)),
                        .bindable_array = &m_array_metset };

      m_stale_metset = false;
    }
    
    // Insert temporary window to modify draw settings
    if (ImGui::Begin("Point draw settings")) {
      ImGui::SliderFloat("Point size", &m_psize, 1.f, 32.f, "%.0f");
    }
    ImGui::End();

    // Get externally shared resources 
    auto &e_viewport_texture = info.get_resource<gl::Texture2d3f>("viewport", "draw_texture");
    auto &e_viewport_arcball = info.get_resource<detail::Arcball>("viewport", "arcball");
    auto &e_viewport_fbuffer = info.get_resource<gl::Framebuffer>("viewport_draw_begin", "frame_buffer_msaa");

    // Declare scoped OpenGL state
    // gl::state::set_viewport(e_viewport_texture.size());
    gl::state::set_op(gl::CullOp::eFront);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp, true),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,  true) };

    // Prepare framebuffer as draw target
    // e_viewport_fbuffer.bind();
    
    // Prepare program and update program uniforms  
    m_program.bind();
    m_program.uniform("u_camera_matrix", e_viewport_arcball.full().matrix());    

    // Dispatch draw calls
    /* if (m_draw_hull.bindable_array) {
      m_program.uniform("u_alpha", .33f);
      gl::dispatch_draw(m_draw_hull);
    } */

    /* if (m_draw_points.bindable_array) {
      m_program.uniform("u_alpha", 1.f);
      gl::state::ScopedSet scope_set(gl::DrawCapability::eDepthTest, true);
      gl::state::set_point_size(m_psize);
      gl::dispatch_draw(m_draw_points);
    } */

    if (m_draw_metset.bindable_array) {
      m_program.uniform("u_alpha", .33f);
      // gl::state::ScopedSet scope_set(gl::DrawCapability::eDepthTest, true);
      gl::dispatch_draw(m_draw_metset);
    }
  }
} // namespace met