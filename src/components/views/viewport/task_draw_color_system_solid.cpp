#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/ray.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_color_system_solid.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;

  void ViewportDrawColorSystemSolid::init(SchedulerHandle &info) {
    met_trace_full();

    // Setup program object
    m_program = {{ .type = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/draw_csys.vert.spv",
                   .cross_path = "resources/shaders/views/draw_csys.vert.json" },
                 { .type = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/draw_csys.frag.spv",
                   .cross_path = "resources/shaders/views/draw_csys.frag.json" }};
   
    // Setup dispatch object for mesh draw, but do not provide mesh data just yet
    m_draw_line = {
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eDepthTest, true },
                           { gl::DrawCapability::eCullOp,   false }},
      .draw_op          = gl::DrawOp::eLine,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };
    m_draw_fill = {
      .type             = gl::PrimitiveType::eTriangles,
      .capabilities     = {{ gl::DrawCapability::eDepthTest, false },
                           { gl::DrawCapability::eCullOp,     true }},
      .draw_op          = gl::DrawOp::eFill,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };
    
    // Instantiate uniform buffers
    m_unif_buffer_line = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_buffer_fill = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map_line    = m_unif_buffer_line.map_as<UniformBuffer>(buffer_access_flags).data();
    m_unif_map_fill    = m_unif_buffer_fill.map_as<UniformBuffer>(buffer_access_flags).data();

    // Instantiate unchanging uniform data
    m_unif_map_line->alpha = 0.25f;
    m_unif_map_line->model_matrix = eig::Matrix4f::Identity();
    m_unif_map_fill->alpha = 0.025f;
    m_unif_map_fill->model_matrix = eig::Matrix4f::Identity();
  }

  void ViewportDrawColorSystemSolid::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external state resources
    const auto &e_proj_state = info.resource("state", "proj_state").read_only<ProjectState>();
    const auto &e_view_state = info.resource("state", "view_state").read_only<ViewportState>();

    // Instantiate new vertex array if mesh data has changed; 
    // this change is extremely rare, so we can afford creating new buffers
    if (auto rsrc = info.resource("gen_color_system_solid", "chull_mesh"); rsrc.is_mutated()) {
      const auto &[e_verts, e_elems, _norms, _uvs] = rsrc.read_only<AlignedMeshData>();

      m_vert_buffer = {{ .data = cnt_span<const std::byte>(e_verts) }};
      m_elem_buffer = {{ .data = cnt_span<const std::byte>(e_elems) }};
      
      m_array = {{
        .buffers = {{ .buffer = &m_vert_buffer,  .index = 0, .stride = sizeof(AlColr) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_elem_buffer
      }};

      m_draw_line.vertex_count = 3 * static_cast<uint>(e_elems.size());
      m_draw_fill.vertex_count = 3 * static_cast<uint>(e_elems.size());
    }
    
    // Update varying uniform data
    if (e_view_state.camera_matrix || e_view_state.camera_aspect) {
      auto &e_arcball = info.resource("viewport.input", "arcball").writeable<detail::Arcball>();
      auto camera_matrix = e_arcball.full().matrix();
      m_unif_map_line->camera_matrix = camera_matrix;
      m_unif_map_fill->camera_matrix = camera_matrix;
      m_unif_buffer_line.flush();
      m_unif_buffer_fill.flush();
    }

    // Experimental clamping code for vertex modification
    if (e_proj_state.verts) {
      // Get external resources
      const auto &e_chull_mesh = info.resource("gen_color_system_solid", "chull_mesh").read_only<AlignedMeshData>();
      const auto &e_chull_cntr = info.resource("gen_color_system_solid", "chull_cntr").read_only<Colr>();

      // Get modified resources
      auto &e_appl_data  = info.global("appl_data").writeable<ApplicationData>();
      auto &e_proj_data  = e_appl_data.project_data;
      
      #pragma omp parallel for
      for (int i = 0; i < e_proj_data.verts.size(); ++i) {
        guard_continue(e_proj_state.verts[i].colr_i);
        auto &vert = e_proj_data.verts[i];

        // Cast a ray through the vertex point towards the mesh centroid;
        Ray ray = { .o = vert.colr_i,
                    .d = (e_chull_cntr - vert.colr_i).matrix().normalized().eval() };
        auto query = raytrace_elem(ray, e_chull_mesh, false);
        guard_continue(query);
        
        // Get relevant element, vertices, and normal
        eig::Array3u el = e_chull_mesh.elems[query.i];
        eig::Vector3f a = e_chull_mesh.verts[el.x()],
                      b = e_chull_mesh.verts[el.y()],
                      c = e_chull_mesh.verts[el.z()];
        eig::Vector3f n = (b - a).cross(c - a).normalized().eval();

        // Test if face is facing towards us, or if we are on the inside
        guard_continue(n.dot(ray.d) < 0.f);

        // Update to intersection point
        vert.colr_i = ray.o + query.t * ray.d;
      }
    }

    // Set shared OpenGL state for coming draw operations
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp, true) };

    // Submit line draw information 
    // m_program.bind("b_uniform", m_unif_buffer_line);
    // gl::dispatch_draw(m_draw_line);
    
    // Submit fill draw information 
    m_program.bind("b_uniform", m_unif_buffer_fill);
    gl::dispatch_draw(m_draw_fill);
  }
} // namespace met