#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/ray.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_csys_ocs.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  ViewportDrawCSysOCSTask::ViewportDrawCSysOCSTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawCSysOCSTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_csys_ocs_mesh = info.get_resource<HalfedgeMesh>("gen_color_solids", "csys_ocs_mesh");
    auto [verts, elems] = generate_data<HalfedgeMeshTraits, AlColr>(e_csys_ocs_mesh);

    // Setup array object and corresponding buffers for mesh data
    m_vert_buffer = {{ .data = cnt_span<const std::byte>(verts) }};
    m_elem_buffer = {{ .data = cnt_span<const std::byte>(elems) }};
    m_array = {{
      .buffers = {{ .buffer = &m_vert_buffer,  .index = 0, .stride = sizeof(AlColr) }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
      .elements = &m_elem_buffer
    }};

    // Setup program object for mesh draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_color_array.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};
   
    // Setup dispatch object for mesh draw
    m_draw = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = 3 * static_cast<uint>(elems.size()),
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    // Setup relevant, non-changing uniforms
    m_program.uniform("u_alpha", .5f);
    m_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
  }

  void ViewportDrawCSysOCSTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Experimental clamping code
    {
      // Get shared resources
      auto &e_chull_mesh = info.get_resource<HalfedgeMesh>("gen_color_solids", "csys_ocs_mesh");
      auto &e_chull_cntr = info.get_resource<Colr>("gen_color_solids", "csys_ocs_cntr");
      auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data  = e_appl_data.project_data;
      
      // Declare an epsilon offset to avoid vertices at [0, 0, 0] doing wonky stuff
      constexpr float eps_offset = 0.0001f;
      eig::Vector3f vec3_offset = 0.5f;

      #pragma omp parallel for
      for (int i = 0; i < e_proj_data.gamut_verts.size(); ++i) {
        auto &vert = e_proj_data.gamut_verts[i];

        // Cast a ray through the vertex point towards the mesh centroid;
        Ray ray = { .o = vert.colr_i,
                    .d = (e_chull_cntr - vert.colr_i).matrix().normalized().eval() };
        auto query = ray_trace_nearest_elem_any_side(ray, e_chull_mesh);
        
        // Test if face is intersected, and even exists
        guard_continue(query);
        auto fh = e_chull_mesh.face_handle(query.i);
        guard_continue(fh.is_valid());

        // Test if face is facing towards us, or if we are on the inside
        auto n = to_eig<float, 3>(e_chull_mesh.calc_face_normal(fh));
        guard_continue(n.dot(ray.d) < 0.f);
        
        // Update to intersection point
        vert.colr_i = ray.o + query.t * ray.d;

        /* Colr curr_pos;
        float curr_dist = std::numeric_limits<float>::max();

        for (auto fh : e_chull_mesh.faces()) {
          // Triangle data
          std::array<eig::Vector3f, 3> p;
          std::ranges::transform(fh.vertices(), p.begin(), 
            [&](auto vh) { return to_eig<float, 3>(e_chull_mesh.point(vh)) + vec3_offset; }); 
          auto centr = ((p[0] + p[1] + p[2]) / 3.f).eval();
          auto norml = to_eig<float, 3>(e_chull_mesh.calc_face_normal(fh));

          // Find distance to closest point on triangle plane; continue on negative distance,
          // as the original point lies below the plane
          float new_dist = (vert.colr_i.matrix() + vec3_offset - centr).dot(norml);
          guard_continue(new_dist >= 0.f);

          // Find closest point on triangle plane
          auto new_pos = (vert.colr_i.matrix() + vec3_offset - norml * new_dist).eval();

          // Find barycentric coordinates of point in relation to triangle; continue on negative
          // coordinates, as the closest point lies outside the triangle
          auto A = (eig::Matrix3f() << p[0], p[1], p[2]).finished();
          auto c = A.colPivHouseholderQr().solve(new_pos).eval();
          c /= c.sum();
          guard_continue((c.array() >= -eps_offset).matrix().all());

          // If this is the closest point, update the current best distance and position
          guard_continue(new_dist < curr_dist);
          curr_dist = new_dist;
          curr_pos = (new_pos - vec3_offset).eval();
          // fmt::print("Caught {}, {}\n", fh.idx(), curr_dist);
        }

        if (curr_dist != std::numeric_limits<float>::max()) {
          // fmt::print("Going from {} to {}\n", vert.colr_i, curr_pos);
          vert.colr_i = curr_pos;
        } */
      }
    }

    // Get shared resources 
    auto &e_arcball = info.get_resource<detail::Arcball>("viewport_input", "arcball");

    // Set OpenGL state for coming draw operations
    gl::state::set_op(gl::CullOp::eBack);
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false) };
    
    // Update varying program uniforms
    m_program.uniform("u_camera_matrix", e_arcball.full().matrix());

    // Submit draw information
    gl::state::set_op(gl::DrawOp::eLine);
    gl::dispatch_draw(m_draw);
    gl::state::set_op(gl::DrawOp::eFill);
  }
} // namespace met