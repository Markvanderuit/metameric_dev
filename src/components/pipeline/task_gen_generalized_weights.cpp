#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/pipeline/task_gen_generalized_weights.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  void GenGeneralizedWeightsTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_colr_data = e_appl_data.loaded_texture;
    const auto &e_proj_data = e_appl_data.project_data;
    
    // Determine compute dispatch size
    const uint dispatch_n    = e_colr_data.size().prod();
    const uint dispatch_ndiv = ceil_div(dispatch_n, 256u / 2u); // tldr; subgroup shenanigans

    // Initialize objects for compute dispatch
    m_program = {{ .type       = gl::ShaderType::eCompute,
                   .spirv_path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights_pre.comp.spv",
                   .cross_path = "resources/shaders/gen_barycentric_weights/gen_barycentric_weights_pre.comp.json" }};
    m_dispatch = { .groups_x = dispatch_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_uniform_map->n = e_appl_data.loaded_texture.size().prod();

    // Initialize mesh buffer data and writeable, flushable mappings where necessary
    gl::Buffer colr_buffer = {{ .data = cast_span<const std::byte>(io::as_aligned((e_colr_data)).data()) }};
    gl::Buffer vert_buffer = {{ .size = buffer_init_size * sizeof(eig::AlArray3f), .flags = buffer_create_flags}};
    gl::Buffer elem_buffer = {{ .size = buffer_init_size * sizeof(eig::AlArray3u), .flags = buffer_create_flags}};
    m_vert_map = vert_buffer.map_as<eig::AlArray3f>(buffer_access_flags);
    m_elem_map = elem_buffer.map_as<eig::AlArray3u>(buffer_access_flags);

    // Initialize buffer holding barycentric weights
    info("colr_buffer").set(std::move(colr_buffer)); // OpenGL buffer storing texture color positions
    info("vert_buffer").set(std::move(vert_buffer)); // OpenGL buffer storing mesh vertex positions
    info("elem_buffer").set(std::move(elem_buffer)); // OpenGL buffer storing mesh element indices
    info("bary_buffer").init<gl::Buffer>({ .size = dispatch_n * sizeof(Bary) }); // Convex weights
  }

  bool GenGeneralizedWeightsTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("state", "proj_state").read_only<ProjectState>().verts;
  }

  void GenGeneralizedWeightsTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Get modified resources
    auto &i_vert_buffer = info("vert_buffer").writeable<gl::Buffer>();
    auto &i_elem_buffer = info("elem_buffer").writeable<gl::Buffer>();

    // Describe ranges over stale mesh vertices/elements
    auto vert_range = std::views::iota(0u, static_cast<uint>(e_proj_state.elems.size()))
                    | std::views::filter([&](uint i) -> bool { return e_proj_state.verts[i]; });
    auto elem_range = std::views::iota(0u, static_cast<uint>(e_proj_state.elems.size()))
                    | std::views::filter([&](uint i) -> bool { return e_proj_state.elems[i]; });
    
    // Push stale vertices/elements
    for (uint i : vert_range) {
      m_vert_map[i] = e_proj_data.verts[i].colr_i;
      i_vert_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }
    for (uint i : elem_range) {
      m_elem_map[i] = e_proj_data.elems[i];
      i_elem_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }

    // Update uniform data
    m_uniform_map->n_verts = e_proj_data.verts.size();
    m_uniform_map->n_elems = e_proj_data.elems.size();
    m_uniform_buffer.flush();

    // Bind required resources
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_vert", i_vert_buffer);
    m_program.bind("b_elem", i_elem_buffer);
    m_program.bind("b_colr", info("colr_buffer").read_only<gl::Buffer>());
    m_program.bind("b_bary", info("bary_buffer").writeable<gl::Buffer>());

    // Dispatch shader to generate generalized barycentric weights
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met