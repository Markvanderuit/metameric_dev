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
    const auto &e_rgb_texture = info.global("app_data").read_only<ApplicationData>().loaded_texture_f32;
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
    m_uniform_map->n = e_appl_data.loaded_texture_f32.size().prod();

    // Initialize buffer holding barycentric weights
    info("colr_buffer").init<gl::Buffer>({ .data = cast_span<const std::byte>(io::as_aligned((e_rgb_texture)).data()) });
    info("elem_buffer").init<gl::Buffer>({ .size = buffer_init_size * sizeof(eig::Array4u), .flags = buffer_create_flags });
    info("bary_buffer").init<gl::Buffer>({ .size = generate_n * sizeof(eig::Array4f) });
  }
  
  bool GenDelaunayWeightsTask::is_active(SchedulerHandle &info) {
    met_trace_full();
    return info("gen_spectral_data", "vert_buffer").is_mutated() ||
           info("gen_spectral_data", "tetr_buffer").is_mutated() ||
           info("gen_spectral_data", "delaunay").is_mutated();
  }

  void GenDelaunayWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("app_data").read_only<ApplicationData>();
    const auto &e_delaunay  = info("gen_spectral_data", "delaunay").read_only<AlignedDelaunayData>();

    // Update uniform data
    m_uniform_map->n_verts = e_delaunay.verts.size();
    m_uniform_map->n_elems = e_delaunay.elems.size();
    m_uniform_buffer.flush();

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_vert", info("gen_spectral_data", "vert_buffer").read_only<gl::Buffer>());
    m_program.bind("b_elem", info("gen_spectral_data", "tetr_buffer").read_only<gl::Buffer>());
    m_program.bind("b_posi", info("colr_buffer").read_only<gl::Buffer>());
    m_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

    // Dispatch shader
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met