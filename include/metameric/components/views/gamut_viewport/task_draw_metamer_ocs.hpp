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
      m_sphere_mesh = generate_unit_sphere<eig::AlArray3f>(3);
      m_hull_vertices = {{ .data = cnt_span<const std::byte>(m_sphere_mesh.verts()), .flags = create_flags }};
      m_hull_elements = {{ .data = cnt_span<const std::byte>(m_sphere_mesh.elems()), .flags = create_flags }};
      
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
      m_hull_dispatch = { .type = gl::PrimitiveType::eTriangles,
                          .vertex_count = (uint) (m_hull_elements.size() / sizeof(uint)),
                          .bindable_array = &m_hull_array,
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

      // Update convex hull mesh if selection has changed, or selected gamut point has changed
      if (m_gamut_idx != e_gamut_idx || e_state_gamut[e_gamut_idx] == CacheState::eStale) {
        m_gamut_idx = e_gamut_idx;
        
        // Get shared resources
        auto &e_ocs_chull = info.get_resource<AlArray3fMesh>("gen_metamer_ocs", fmt::format("ocs_chull_{}", m_gamut_idx));

        // Copy new data to buffer and adjust vertex draw count
        m_hull_vertices.set(cnt_span<const std::byte>(e_ocs_chull.verts()), e_ocs_chull.verts().size() * sizeof(AlArray3fMesh::Vert));
        m_hull_elements.set(cnt_span<const std::byte>(e_ocs_chull.elems()), e_ocs_chull.elems().size() * sizeof(AlArray3fMesh::Elem));
        m_hull_dispatch.vertex_count = e_ocs_chull.elems().size() * 3;
      }

      // Declare scoped OpenGL state
      gl::state::set_op(gl::CullOp::eBack);
      gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
      auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                                 gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true),
                                 gl::state::ScopedSet(gl::DrawCapability::eCullOp,    true),
                                 gl::state::ScopedSet(gl::DrawCapability::eDepthTest, false) };
                                 
      // Set model/camera translations
      eig::Affine3f transl(eig::Translation3f(-e_ocs_centr .matrix().eval()));
      m_program.uniform("u_model_matrix",  transl.matrix());
      m_program.uniform("u_camera_matrix", e_arcball.full().matrix());    

      // Dispatch draw
      gl::dispatch_draw(m_hull_dispatch);
    }
  };
} // namespace met