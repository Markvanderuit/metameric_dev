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
    gl::Buffer    m_point_vertices;
    gl::Array     m_point_array;
    gl::DrawInfo  m_point_dispatch;
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
      const auto spheroid_mesh = generate_spheroid<HalfedgeMeshTraits>(3);
      m_hull_vertices = {{ .size = spheroid_mesh.n_vertices() * sizeof(eig::AlArray3f), .flags = create_flags }};
      m_hull_elements = {{ .size = spheroid_mesh.n_faces() * sizeof(eig::Array3u), .flags = create_flags }};
      m_point_vertices = {{ .size = 32 * sizeof(eig::AlArray3f), .flags = create_flags }};

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
      m_point_array = {{
        .buffers = {{ .buffer = &m_point_vertices, .index = 0, .stride = sizeof(AlColr) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }}
      }};
      m_hull_dispatch = { .type = gl::PrimitiveType::eTriangles,
                          .vertex_count = (uint) (m_hull_elements.size() / sizeof(uint)),
                          .bindable_array = &m_hull_array,
                          .bindable_program = &m_program };
      m_point_dispatch = { .type = gl::PrimitiveType::ePoints,
                           .vertex_count = (uint) (m_point_vertices.size() / sizeof(eig::AlArray3f)),
                           .bindable_array = &m_point_array,
                           .bindable_program = &m_program };

      // Set non-changing uniform values
      m_program.uniform("u_alpha", .66f);

      // Set selection to "none"
      m_gamut_idx = -1;
    }
    
    void eval(detail::TaskEvalInfo &info) override {
      met_trace_full();
      constexpr auto task_begin_fmt = FMT_COMPILE("{}_draw_begin");

      // Verify that a gamut point is selected before continuing
      auto &e_gamut_ind = info.get_resource<std::vector<uint>>("viewport_input", "gamut_selection");
      int   e_gamut_idx = e_gamut_ind.size() == 1 ? e_gamut_ind[0] : -1;
      guard(e_gamut_idx >= 0);

      // Get shared resources
      auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_state_gamut  = info.get_resource<std::array<CacheState, 4>>("project_state", "gamut_summary");
      auto &e_arcball      = info.get_resource<detail::Arcball>(m_parent, "arcball");
      auto &e_ocs_centr    = info.get_resource<Colr>("gen_metamer_ocs", fmt::format("ocs_center_{}", e_gamut_idx));
      auto &e_ocs_points   = info.get_resource<std::vector<eig::AlArray3f>>("gen_metamer_ocs", fmt::format("ocs_points_{}", e_gamut_idx));

      // Update convex hull mesh if selection has changed, or selected gamut point has changed
      if (m_gamut_idx != e_gamut_idx || e_state_gamut[e_gamut_idx] == CacheState::eStale) {
        m_gamut_idx = e_gamut_idx;
        
        // Get shared resources
        auto &e_ocs_chull = info.get_resource<HalfedgeMesh>("gen_metamer_ocs", fmt::format("ocs_chull_{}", m_gamut_idx));
        auto [verts, elems] = generate_data<HalfedgeMeshTraits, eig::AlArray3f>(e_ocs_chull);

        // Copy new data to buffer and adjust vertex draw count
        m_hull_vertices.set(cnt_span<const std::byte>(verts), verts.size() * sizeof(decltype(verts)::value_type));
        m_hull_elements.set(cnt_span<const std::byte>(elems), elems.size() * sizeof(decltype(elems)::value_type));
        m_hull_dispatch.vertex_count = e_ocs_chull.n_faces() * 3;
        m_point_vertices.set(cnt_span<const std::byte>(e_ocs_points), e_ocs_points.size() * sizeof(eig::AlArray3f));
        m_point_dispatch.vertex_count = e_ocs_points.size();
      }

      // Declare scoped OpenGL state
      gl::state::set_op(gl::CullOp::eFront);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
      gl::state::set_point_size(8.f);
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false) };
                                 
      // Set model/camera translations
      eig::Affine3f transl(eig::Translation3f(-e_ocs_centr .matrix().eval()));
      m_program.uniform("u_model_matrix",  transl.matrix());
      m_program.uniform("u_camera_matrix", e_arcball.full().matrix());    

      // Dispatch draw
      m_program.uniform("u_alpha", .66f);
      gl::dispatch_draw(m_hull_dispatch);
      m_program.uniform("u_alpha", 1.f);
      gl::dispatch_draw(m_point_dispatch);
    }
  };
} // namespace met