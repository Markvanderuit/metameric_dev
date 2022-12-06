#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/viewport/task_draw_mvc.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/arcball.hpp>

namespace met {
  constexpr std::array<float, 2 * 4> verts = {
    -1.f, -1.f,
     1.f, -1.f,
     1.f,  1.f,
    -1.f,  1.f
  };

  constexpr std::array<uint, 2 * 3> elems = {
    0, 1, 2,
    2, 3, 0
  };

  constexpr float point_psize = 0.002f;
  constexpr uint n_points_per_dim = 32;
  constexpr uint n_points = n_points_per_dim * n_points_per_dim * n_points_per_dim;

  ViewportDrawMVCTask::ViewportDrawMVCTask(const std::string &name)
  : detail::AbstractTask(name, true) { }

  void ViewportDrawMVCTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    const eig::Array3u generate_n    = n_points_per_dim;
    const eig::Array3u generate_ndiv = ceil_div(generate_n, 8u);

    // Setup objects to compute interpolated colors on a point grid
    m_baryc_posi_buffer = {{ .size = n_points * sizeof(eig::AlArray3f) }};
    m_baryc_colr_buffer = {{ .size = n_points * sizeof(eig::AlArray3f) }};
    m_baryc_program = {{ .type = gl::ShaderType::eCompute,
                         .path = "resources/shaders/draw_mvc/draw_mvc_generate.comp.spv_opt",
                         .is_spirv_binary = true }};
    m_baryc_dispatch = { .groups_x = generate_ndiv[0], 
                         .groups_y = generate_ndiv[1], 
                         .groups_z = generate_ndiv[2], 
                         .bindable_program = &m_baryc_program }; 

    // Initialize uniform buffers
    const auto create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
    const auto map_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;
    m_baryc_uniform_buffer = {{ .data = obj_span<const std::byte>(generate_n) }};
    m_baryc_bary_buffer = {{ .size = sizeof(BarycentricBuffer), .flags = create_flags}};
    m_baryc_bary_map = &m_baryc_bary_buffer.map_as<BarycentricBuffer>(map_flags)[0];

    // Setup objects to draw interpolated color points using instanced quad draw
    m_points_vert_buffer = {{ .data = cnt_span<const std::byte>(verts) }};
    m_points_elem_buffer = {{ .data = cnt_span<const std::byte>(elems) }};
    m_points_array = {{
      .buffers = {{ .buffer = &m_points_vert_buffer,    .index = 0, .stride = 2 * sizeof(float),  .divisor = 0 },
                  { .buffer = &m_baryc_posi_buffer, .index = 1, .stride = sizeof(eig::AlArray3f), .divisor = 1 },
                  { .buffer = &m_baryc_colr_buffer, .index = 2, .stride = sizeof(eig::Array4f),   .divisor = 1 }},
      .attribs = {{ .attrib_index = 0, .buffer_index = 0, .size = gl::VertexAttribSize::e2 },
                  { .attrib_index = 1, .buffer_index = 1, .size = gl::VertexAttribSize::e3 },
                  { .attrib_index = 2, .buffer_index = 2, .size = gl::VertexAttribSize::e4 }},
      .elements = &m_points_elem_buffer
    }};
    m_points_program = {{ .type = gl::ShaderType::eVertex,   .path = "resources/shaders/draw_mvc/draw_mvc.vert" },
                        { .type = gl::ShaderType::eFragment, .path = "resources/shaders/draw_mvc/draw_mvc.frag" }};
    m_points_dispatch = {
      .type             = gl::PrimitiveType::eTriangles,
      .vertex_count     = elems.size(),
      .instance_count   = n_points,
      .bindable_array   = &m_points_array,
      .bindable_program = &m_points_program
    };

    // Set constant uniforms
    m_points_program.uniform("u_point_radius", point_psize);
  }

  void ViewportDrawMVCTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data      = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_color_gamut_c = e_app_data.project_data.gamut_colr_i;
    auto &e_arcball       = info.get_resource<detail::Arcball>("viewport_input", "arcball");
    auto &e_color_gamut   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "colr_buffer");
    auto &e_elems_gamut   = info.get_resource<gl::Buffer>("gen_spectral_gamut", "elem_buffer");

    // Declare scoped OpenGL state
    auto draw_capabilities = { gl::state::ScopedSet(gl::DrawCapability::eMSAA,      true),
                               gl::state::ScopedSet(gl::DrawCapability::eDepthTest, true),
                               gl::state::ScopedSet(gl::DrawCapability::eBlendOp,   true) };

    // Update barycentric coordinate inverse matrix in mapped uniform buffer
    m_baryc_bary_map->sub = e_color_gamut_c[3];
    m_baryc_bary_map->inv.block<3, 3>(0, 0) = (eig::Matrix3f() 
      << e_color_gamut_c[0] - e_color_gamut_c[3], 
         e_color_gamut_c[1] - e_color_gamut_c[3], 
         e_color_gamut_c[2] - e_color_gamut_c[3]).finished().inverse().eval();
    m_baryc_bary_buffer.flush();
                             
    // Update varying program uniforms
    eig::Matrix4f camera_matrix = e_arcball.full().matrix();
    eig::Vector2f camera_aspect = { 1.f, e_arcball.m_aspect };
    m_points_program.uniform("u_camera_matrix", camera_matrix);
    m_points_program.uniform("u_billboard_aspect", camera_aspect);  

    // Bind resources to buffer targets
    e_color_gamut.bind_to(gl::BufferTargetType::eShaderStorage,       0);
    e_elems_gamut.bind_to(gl::BufferTargetType::eShaderStorage,       1);
    m_baryc_posi_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    m_baryc_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 3);
    m_baryc_uniform_buffer.bind_to(gl::BufferTargetType::eUniform, 0);
    m_baryc_bary_buffer.bind_to(gl::BufferTargetType::eUniform,    1);

    // Generate interpolated colors per grid point
    // gl::dispatch_compute(m_baryc_dispatch);
    // gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);

    // Draw grid points with interpolated colors
    // gl::dispatch_draw(m_points_dispatch);
  }
} // namespace met