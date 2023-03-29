#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_delaunay_weights.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  void GenDelaunayWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_rgb_texture = info.global("app_data").read_only<ApplicationData>().loaded_texture;
    const auto &e_appl_data   = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;

    const uint generate_n    = e_rgb_texture.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);

    // Initialize objects for shader call
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/gen_barycentric_weights/gen_delaunay_weights.comp.spv",
                   .cross_path = "resources/shaders/gen_barycentric_weights/gen_delaunay_weights.comp.json" }};
    m_dispatch = { .groups_x = generate_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = &m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];
    m_uniform_map->n = e_appl_data.loaded_texture.size().prod();

    // Initialize mesh buffer data and writeable, flushable mappings where necessary
    gl::Buffer colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data()) }};
    gl::Buffer vert_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4f), .flags = buffer_create_flags}};
    gl::Buffer elem_buffer = {{ .size = buffer_init_size * sizeof(eig::Array4u), .flags = buffer_create_flags}};
    m_vert_map = vert_buffer.map_as<eig::AlArray3f>(buffer_access_flags);
    m_elem_map = elem_buffer.map_as<eig::Array4u>(buffer_access_flags);

    // Initialize buffer holding barycentric weights
    info("delaunay").set<AlignedDelaunayData>({ });  // Generated delaunay tetrahedralization over input vertices
    info("colr_buffer").set(std::move(colr_buffer)); // OpenGL buffer storing texture color positions
    info("vert_buffer").set(std::move(vert_buffer)); // OpenGL buffer storing delaunay vertex positions
    info("elem_buffer").set(std::move(elem_buffer)); // OpenGL buffer storing delaunay tetrahedral elements
    info("bary_buffer").init<gl::Buffer>({ .size = generate_n * sizeof(eig::Array4f) }); // Convex weights
  }
  
  bool GenDelaunayWeightsTask::is_active(SchedulerHandle &info) {
    met_trace_full();
    return info("state", "pipeline_state").read_only<ProjectState>().any_verts;
  }

  void GenDelaunayWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("app_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    // Get modified resources
    auto &i_delaunay    = info("delaunay").writeable<AlignedDelaunayData>();
    auto &i_vert_buffer = info("vert_buffer").writeable<gl::Buffer>();
    auto &i_elem_buffer = info("elem_buffer").writeable<gl::Buffer>();

    // Generate new delaunay structure
    std::vector<Colr> delaunay_input(e_proj_data.vertices.size());
    std::ranges::transform(e_proj_data.vertices, delaunay_input.begin(), [](const auto &vt) { return vt.colr_i; });
    i_delaunay = generate_delaunay<AlignedDelaunayData, Colr>(delaunay_input);

    // Push buffer data // TODO optimize?
    m_uniform_map->n_verts = i_delaunay.verts.size();
    m_uniform_map->n_elems = i_delaunay.elems.size();
    std::ranges::copy(i_delaunay.verts, m_vert_map.begin());
    std::ranges::copy(i_delaunay.elems, m_elem_map.begin());
    i_vert_buffer.flush(i_delaunay.verts.size() * sizeof(eig::AlArray3f));
    i_elem_buffer.flush(i_delaunay.elems.size() * sizeof(eig::Array4u));
    m_uniform_buffer.flush();

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_vert", i_vert_buffer);
    m_program.bind("b_elem", i_elem_buffer);
    m_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
    m_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

    // Dispatch shader
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met