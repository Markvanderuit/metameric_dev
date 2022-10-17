#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/scheduler_task.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/array.hpp>
#include <small_gl/buffer.hpp>
#include <small_gl/dispatch.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/program.hpp>

namespace met {
  class DrawOcsTask : public detail::AbstractTask {
    std::string  m_parent;
    gl::Program  m_program;
    gl::Array    m_array_hull;
    gl::Array    m_array_points;
    gl::DrawInfo m_dispatch_hull;
    gl::DrawInfo m_dispatch_points;
    bool         m_stale;
    uint         m_buffer_i;
    
  public:
    DrawOcsTask(const std::string &name, const std::string &parent)
    : detail::AbstractTask(name, true),
      m_parent(parent) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Construct draw components
      m_program = {{ .type = gl::ShaderType::eVertex, 
                     .path = "resources/shaders/viewport/draw_color_array.vert" },
                   { .type = gl::ShaderType::eFragment,  
                     .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};

      // Set non-changing uniform values
      m_program.uniform("u_alpha", .66f);
    }
    
    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
      constexpr auto task_begin_fmt = FMT_COMPILE("{}_draw_begin");

      // Get shared resources 
      auto &e_ocs_buffer = info.get_resource<gl::Buffer>("gen_ocs", "ocs_buffer");
      if (!e_ocs_buffer.is_init() || m_buffer_i != e_ocs_buffer.object()) {
        m_stale = true;
      }

      // New buffer; recreate vertex array and draw commands
      if (m_stale && e_ocs_buffer.is_init()) {
        auto &e_ocs_verts  = info.get_resource<gl::Buffer>("gen_ocs", "ocs_verts");
        auto &e_ocs_elems  = info.get_resource<gl::Buffer>("gen_ocs", "ocs_elems");

        // Instantiate vertex array objects
        m_array_points = {{
          .buffers = {{ .buffer = &e_ocs_buffer, .index = 0, .stride = sizeof(AlColr) }},
          .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
        }};
        m_array_hull = {{
          .buffers = {{ .buffer = &e_ocs_verts, .index = 0, .stride = sizeof(AlColr) }},
          .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
          .elements = &e_ocs_elems
        }};

        // Instantiate dispatch information
        m_dispatch_hull = { .type = gl::PrimitiveType::eTriangles,
                            .vertex_count = (uint) (e_ocs_elems.size() / sizeof(uint)),
                            .bindable_array = &m_array_hull,
                            .bindable_program = &m_program };
        m_dispatch_points = { .type = gl::PrimitiveType::ePoints,
                              .vertex_count = (uint) (e_ocs_buffer.size() / sizeof(AlColr)),
                              .bindable_array = &m_array_points,
                              .bindable_program = &m_program };

        m_buffer_i = e_ocs_buffer.object();
        m_stale    = false;
      }

      // Continue only if draw data is no longer stale
      guard(!m_stale);
      guard(m_dispatch_hull.bindable_array);

      // Get shared resources 
      auto &e_arcball    = info.get_resource<detail::Arcball>(m_parent, "arcball");
      auto &e_fbuffer    = info.get_resource<gl::Framebuffer>(fmt::format(task_begin_fmt, m_parent), "frame_buffer_ms");
      auto &e_gamut_idx  = info.get_resource<int>("viewport", "gamut_selection");
      auto &e_app_data   = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_colr = e_app_data.project_data.gamut_colr_i[e_gamut_idx];
      auto &e_ocs_centr  = info.get_resource<Colr>("gen_ocs", "ocs_centr");

      // Declare scoped OpenGL state
      gl::state::set_point_size(8.f);
      gl::state::set_op(gl::CullOp::eFront);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,    true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp, true),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,  true) };
      
      // Describe inverse translation to center
      eig::Affine3f transl(eig::Translation3f(eig::Vector3f(0.5f) - e_ocs_centr.matrix()));
      
      // Update program uniform data
      m_program.uniform("u_model_matrix",  transl.matrix());
      m_program.uniform("u_camera_matrix", e_arcball.full().matrix());    

      // Dispatch point draw
      gl::dispatch_draw(m_dispatch_hull);
      gl::dispatch_draw(m_dispatch_points);
    }
  };
} // namespace met