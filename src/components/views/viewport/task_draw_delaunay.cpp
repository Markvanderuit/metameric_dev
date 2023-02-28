#include <metameric/core/data.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_delaunay.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <small_gl/framebuffer.hpp>
#include <small_gl/utility.hpp>

namespace met {
  ViewportDrawDelaunayTask::ViewportDrawDelaunayTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawDelaunayTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data = e_appl_data.project_data;

    // Generate delaunay tetrahedralization of input data
    // std::vector<Colr> input_verts(e_proj_data.vertices.size());
    // std::ranges::transform(e_proj_data.vertices, input_verts.begin(), [](const auto &v) { return v.colr_i; });
    // auto [verts, elems] = generate_delaunay<AlignedDelaunayData, Colr>(input_verts);

    // // Push to buffer
    // m_vert_buffer = {{ .data = cnt_span<const std::byte>(verts) }};
    // m_elem_buffer = {{ .data = cnt_span<const std::byte>(elems) }};
    // m_array = {{
    //   .buffers = {{ .buffer = &m_vert_buffer,  .index = 0, .stride = sizeof(AlColr) }},
    //   .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
    //   .elements = &m_elem_buffer
    // }};

    // Setup program object for mesh draw
    m_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/viewport/draw_color_array.vert" },
                 { .type = gl::ShaderType::eFragment, .path = "resources/shaders/viewport/draw_color_uniform_alpha.frag" }};
   
    // Setup dispatch object for mesh draw
    m_draw_line = {
      .type             = gl::PrimitiveType::eTriangles,
      .draw_op          = gl::DrawOp::eLine,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };
    m_draw_fill = {
      .type             = gl::PrimitiveType::eTriangles,
      .draw_op          = gl::DrawOp::eFill,
      .bindable_array   = &m_array,
      .bindable_program = &m_program
    };

    // Setup relevant, non-changing uniforms
    m_program.uniform("u_model_matrix", eig::Matrix4f::Identity().eval());
  }

  void ViewportDrawDelaunayTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get state objects
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    auto &e_view_state = info.get_resource<ViewportState>("state", "viewport_state");

    if (e_pipe_state.any_verts || !m_array.is_init()) {
      auto &e_vert_buffer = info.get_resource<gl::Buffer>("gen_spectral_data", "vert_buffer");
      auto &e_delaunay    = info.get_resource<AlignedDelaunayData>("gen_spectral_data", "delaunay");
      auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data   = e_appl_data.project_data;

      // Generate delaunay tetrahedralization of input data
      auto trimesh = convert_mesh<AlignedMeshData>(convert_mesh<IndexedDelaunayData>(e_delaunay));

      // Push to buffer
      m_elem_buffer = {{ .data = cnt_span<const std::byte>(trimesh.elems) }};
      m_array = {{
        .buffers = {{ .buffer = &e_vert_buffer,  .index = 0, .stride = sizeof(AlColr) }},
        .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e3 }},
        .elements = &m_elem_buffer
      }};

      m_draw_line.vertex_count = 3 * trimesh.elems.size();
      m_draw_fill.vertex_count = 3 * trimesh.elems.size();
    }

    // Set shared OpenGL state for coming draw operations
    gl::state::set_op(gl::BlendOp::eSrcAlpha, gl::BlendOp::eOneMinusSrcAlpha);
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,       true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,    true),
                               gl::state::ScopedSet(gl::DrawCapability::eCullOp,    false),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest,  true) };
    
    // Update varying program uniforms
    if (e_view_state.camera_matrix || e_view_state.camera_aspect) {
      auto &e_arcball = info.get_resource<detail::Arcball>("viewport_input", "arcball");
      m_program.uniform("u_camera_matrix", e_arcball.full().matrix());
    }

    // Submit draw information with varying alpha
    m_program.uniform("u_alpha", 1.f);
    gl::dispatch_draw(m_draw_line);
    m_program.uniform("u_alpha", .1f);
    gl::dispatch_draw(m_draw_fill);
  }
} // namespace met