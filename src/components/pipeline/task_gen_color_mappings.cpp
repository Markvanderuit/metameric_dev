#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_color_mappings.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  GenColorMappingTask::GenColorMappingTask(uint mapping_i)
  : m_mapping_i(mapping_i) { }

  void GenColorMappingTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
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
    m_gamut_buffer = {{ .size = buffer_init_size * sizeof(AlColr), .flags = buffer_create_flags }};
    m_gamut_map    = m_gamut_buffer.map_as<AlColr>(buffer_access_flags);

    // Set up uniform buffer and establish a flushable mapping
    m_uniform_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_uniform_map    = m_uniform_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_uniform_map->n = e_appl_data.loaded_texture.size().prod();

    // Create color buffer output for this task
    info("colr_buffer").init<gl::Buffer>({ .size  = (size_t) dispatch_n * sizeof(AlColr)  });

    m_init_stale = true;
  }

  bool GenColorMappingTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    return m_init_stale || e_proj_state.csys[m_mapping_i] || e_proj_state.verts;
  }

  void GenColorMappingTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    const auto &e_vert_spec  = info("gen_spectral_data", "spectra").read_only<std::vector<Spec>>();

    // Update uniform data
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      m_uniform_map->n_verts = e_proj_data.verts.size();
      m_uniform_map->n_elems = e_proj_data.elems.size();
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const auto &e_delaunay = info("gen_convex_weights", "delaunay").read_only<AlignedDelaunayData>();
      m_uniform_map->n_verts = e_delaunay.verts.size();
      m_uniform_map->n_elems = e_delaunay.elems.size();
    }
    m_uniform_buffer.flush();
  
    // Push gamut data, given any state change
    ColrSystem csys = e_proj_data.csys(m_mapping_i);
    for (uint i = 0; i < e_proj_data.verts.size(); ++i) {
      guard_continue(m_init_stale || e_proj_state.csys[m_mapping_i] || e_proj_state.verts[i]);
      m_gamut_map[i] = csys.apply_color_indirect(e_vert_spec[i]);
      m_gamut_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_uniform_buffer);
    m_program.bind("b_bary", info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>());
    m_program.bind("b_vert", m_gamut_buffer);
    m_program.bind("b_elem", info("gen_convex_weights", "elem_buffer").read_only<gl::Buffer>());
    m_program.bind("b_colr", info("colr_buffer").writeable<gl::Buffer>());

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch);

    m_init_stale = false;
  }

  void GenColorMappingsTask::init(SchedulerHandle &info) {
    met_trace();

    // Get external resources
    const auto &e_proj_data = info.global("appl_data").read_only<ApplicationData>().project_data;

    // Add subtasks to perform mapping
    m_mapping_subtasks.init(info, e_proj_data.color_systems.size(), 
      [](uint i)         { return fmt::format("gen_mapping_{}", i); },
      [](auto &, uint i) { return GenColorMappingTask(i); });
  }

  void GenColorMappingsTask::eval(SchedulerHandle &info) {
    met_trace();
    
    // Get external resources
    const auto &e_proj_data = info.global("appl_data").read_only<ApplicationData>().project_data;

    // Adjust nr. of subtasks
    m_mapping_subtasks.eval(info, e_proj_data.color_systems.size());
  }
} // namespace met