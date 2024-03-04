#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/dev/task_gen_random_mappings.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  GenRandomMappingTask::GenRandomMappingTask(uint constraint_i, uint mapping_i)
  : m_constraint_i(constraint_i),
    m_mapping_i(mapping_i) { }

  void GenRandomMappingTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").getr<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    // Initialize dispatch objects
    const uint dispatch_n = e_appl_data.loaded_texture.size().prod();
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      const uint dispatch_ndiv = ceil_div(dispatch_n, 256u / (generalized_weights / 4));
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .spirv_path = "resources/shaders/pipeline/gen_color_mapping_generalized.comp.spv",
                     .cross_path = "resources/shaders/pipeline/gen_color_mapping_generalized.comp.json" }};
      m_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_program };
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .spirv_path = "resources/shaders/pipeline/gen_color_mapping_delaunay.comp.spv",
                     .cross_path = "resources/shaders/pipeline/gen_color_mapping_delaunay.comp.json" }};
      m_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_program };
    }

    // Set up gamut buffer and establish a flushable mapping
    m_vert_buffer = {{ .size = buffer_init_size * sizeof(AlColr), .flags = buffer_create_flags }};
    m_vert_map    = m_vert_buffer.map_as<AlColr>(buffer_access_flags);

    // Set up uniform buffer and establish a flushable mapping
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_unif_map->n = e_appl_data.loaded_texture.size().prod();

    // Create color buffer output for this task
    info("colr_buffer").init<gl::Buffer>({ .size  = (size_t) dispatch_n * sizeof(AlColr)  });

    m_has_run_once = false;
  }

  bool GenRandomMappingTask::is_active(SchedulerHandle &info) {
    met_trace();
    auto rsrc = info("gen_random_constraints", "constraints");
    return rsrc.is_init() && (rsrc.is_mutated() || !m_has_run_once);
  }

  void GenRandomMappingTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data   = info.global("appl_data").getr<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_proj_state  = info("state", "proj_state").getr<ProjectState>();
    const auto &e_verts       = info("gen_random_constraints", "constraints").getr<
      std::vector<std::vector<ProjectData::Vert>>
    >().at(m_constraint_i);
    const auto &e_vert_slct   = info("viewport.input.vert", "selection").getr<std::vector<uint>>();
    const auto &e_vert_spec   = info("gen_spectral_data", "spectra").getr<std::vector<Spec>>();

    // Update uniform data
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      m_unif_map->n_verts = e_proj_data.verts.size();
      m_unif_map->n_elems = e_proj_data.elems.size();
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const auto &e_delaunay = info("gen_convex_weights", "delaunay").getr<AlDelaunay>();
      m_unif_map->n_verts = e_delaunay.verts.size();
      m_unif_map->n_elems = e_delaunay.elems.size();
    }
    m_unif_buffer.flush();

    // Obtain differense of all vertex indices and selected vertex indices
    auto vert_diff = std::views::iota(0u, static_cast<uint>(e_verts.size()))
                   | std::views::filter([&](uint i) { return std::ranges::find(e_vert_slct, i) == e_vert_slct.end(); });

    // Push unselected/selected gamut data separately
    ColrSystem mapping_csys = e_proj_data.csys(m_mapping_i);
    for (uint i : vert_diff)
      m_vert_map[i] = mapping_csys.apply(e_vert_spec[i]);
    for (uint i : e_vert_slct)
      m_vert_map[i] = e_verts[i].colr_j[0];
    m_vert_buffer.flush();

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_unif_buffer);
    m_program.bind("b_bary", info("gen_convex_weights", "bary_buffer").getr<gl::Buffer>());
    m_program.bind("b_vert", m_vert_buffer);
    m_program.bind("b_elem", info("gen_convex_weights", "elem_buffer").getr<gl::Buffer>());
    m_program.bind("b_colr", info("colr_buffer").getw<gl::Buffer>());

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch);

    m_has_run_once = true;
  }

  void GenRandomMappingsTask::init(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_constraints = info("gen_random_constraints", "constraints").getr<std::vector<std::vector<ProjectData::Vert>>>();

    // Add subtasks to perform mapping
    m_mapping_subtasks.init(info, e_constraints.size(), 
      [](uint i)         { return fmt::format("gen_mapping_{}", i); },
      [](auto &, uint i) { return GenRandomMappingTask(i, 1); });
  }

  void GenRandomMappingsTask::eval(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_constraints = info("gen_random_constraints", "constraints").getr<std::vector<std::vector<ProjectData::Vert>>>();

    // Adjust nr. of subtasks
    m_mapping_subtasks.eval(info, e_constraints.size());
  }
} // namespace met