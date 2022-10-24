#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
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
#include <small_gl/texture.hpp>
#include <fmt/ranges.h>

namespace met {
  class DrawMetamerOCSTask : public detail::AbstractTask {
    std::string   m_parent;
    int           m_gamut_idx;
    AlArray3fMesh m_sphere_mesh;
    AlArray3fWireframe 
                  m_sphere_mesh_wf;
    gl::Buffer    m_hull_vertices;
    gl::Buffer    m_hull_elements;
    gl::Array     m_hull_array;
    gl::DrawInfo  m_hull_dispatch;
    gl::Program   m_program;

  public:
    DrawMetamerOCSTask(const std::string &name, const std::string &parent)
    : detail::AbstractTask(name, true),
      m_parent(parent) { }

    void init(detail::TaskInitInfo &info) override {
      met_trace_full();

      // Generate a uv sphere mesh for convex hull approximation and create gpu buffers
      constexpr auto create_flags = gl::BufferCreateFlags::eStorageDynamic;
      m_sphere_mesh = generate_unit_sphere<eig::AlArray3f>();
      m_sphere_mesh_wf = generate_wireframe<eig::AlArray3f>(m_sphere_mesh);
      m_hull_vertices = {{ .data = cnt_span<const std::byte>(m_sphere_mesh_wf.verts()), .flags = create_flags }};
      m_hull_elements = {{ .data = cnt_span<const std::byte>(m_sphere_mesh_wf.elems()), .flags = create_flags }};
      
      // Construct non-changing draw components
      m_program = {{ .type = gl::ShaderType::eVertex, 
                     .path = "resources/shaders/viewport/draw_color_array.vert" },
                   { .type = gl::ShaderType::eFragment,  
                     .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};
      m_hull_array = {{
        .buffers = {{ .buffer = &m_hull_vertices, .index = 0, .stride = sizeof(AlColr) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_hull_elements
      }};
      m_hull_dispatch = { .type = gl::PrimitiveType::eLines,
                          .vertex_count = (uint) (m_hull_elements.size() / sizeof(uint)),
                          .bindable_array = &m_hull_array,
                          .bindable_program = &m_program };

      // Set non-changing uniform values
      eig::Affine3f transl(eig::Translation3f(eig::Vector3f(0.f)));
      m_program.uniform("u_alpha", .66f);
      m_program.uniform("u_model_matrix",  transl.matrix());

      // Set selection to "none"
      m_gamut_idx = -1;
    }
    
    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
      constexpr auto task_begin_fmt = FMT_COMPILE("{}_draw_begin");

      // Verify that a gamut point is selected before continuing
      auto &e_gamut_idx  = info.get_resource<int>("viewport", "gamut_selection");
      guard(e_gamut_idx >= 0);

      // Get shared resources
      auto &e_app_data       = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_gamut_mapp_i   = e_app_data.project_data.gamut_mapp_i;
      auto &e_gamut_mapp_j   = e_app_data.project_data.gamut_mapp_j;
      auto &e_state_gamut    = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_summary");
      auto &e_state_mappings = info.get_resource<std::vector<CacheState>>("project_state", "mappings");
      auto &e_arcball        = info.get_resource<detail::Arcball>(m_parent, "arcball");
      
      // Update convex hull mesh if selection has changed, or selected gamut point has changed
      if (m_gamut_idx                                   != e_gamut_idx        ||
          e_state_gamut[e_gamut_idx]                    == CacheState::eStale ||
          e_state_mappings[e_gamut_mapp_i[e_gamut_idx]] == CacheState::eStale ||
          e_state_mappings[e_gamut_mapp_j[e_gamut_idx]] == CacheState::eStale) {
        m_gamut_idx = e_gamut_idx;
        
        // Get shared resources
        auto &e_ocs_points = info.get_resource<std::vector<eig::AlArray3f>>("gen_metamer_ocs", fmt::format("ocs_points_{}", m_gamut_idx));

        // Generate approximate convex hull around points and copy to gl buffer
        auto hull = generate_convex_hull<eig::AlArray3f>(m_sphere_mesh, e_ocs_points);
        auto hull_wf = generate_wireframe<eig::AlArray3f>(hull);
        m_hull_vertices.set(cnt_span<const std::byte>(hull_wf.verts()), hull_wf.verts().size() * sizeof(decltype(hull_wf)::Vert));
        m_hull_elements.set(cnt_span<const std::byte>(hull_wf.elems()), hull_wf.elems().size() * sizeof(decltype(hull_wf)::Elem));

        // Adjust vertex count in case convex hull is smaller
        m_hull_dispatch.vertex_count = hull_wf.elems().size() * 2;
      }

      // Declare scoped OpenGL state
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false) };

      // Dispatch draw
      m_program.uniform("u_camera_matrix", e_arcball.full().matrix());    
      gl::dispatch_draw(m_hull_dispatch);
    }
  };
} // namespace met