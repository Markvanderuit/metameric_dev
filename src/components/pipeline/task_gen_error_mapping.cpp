#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/pipeline/task_gen_error_mapping.hpp>
#include <small_gl/utility.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;
  
  GenErrorMappingTask::GenErrorMappingTask(uint mapping_i)
  : m_mapping_i(mapping_i) { }

  void GenErrorMappingTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data = info.global("appl_data").getr<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    
    // Initialize program object
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .spirv_path = "resources/shaders/pipeline/gen_error_mapping_generalized.comp.spv",
                     .cross_path = "resources/shaders/pipeline/gen_error_mapping_generalized.comp.json" }};
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .spirv_path = "resources/shaders/pipeline/gen_error_mapping_delaunay.comp.spv",
                     .cross_path = "resources/shaders/pipeline/gen_error_mapping_delaunay.comp.json" }};
    }

    // Set up gamut buffer and establish a flushable mapping
    m_vert_buffer = {{ .size = buffer_init_size * sizeof(AlColr), .flags = buffer_create_flags }};
    m_vert_map    = m_vert_buffer.map_as<AlColr>(buffer_access_flags);

    // Set up uniform buffer and establish a flushable mapping
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_unif_map->size_in = e_appl_data.loaded_texture.size();
    
    // Lazy init texture-related components
    set_texture_info(info, { .size = 64 });
  }

  bool GenErrorMappingTask::is_active(SchedulerHandle &info) {
    met_trace();
    const auto &e_proj_state = info("state", "proj_state").getr<ProjectState>();
    return m_is_mutated || e_proj_state.csys[m_mapping_i] || e_proj_state.verts;
  }

  void GenErrorMappingTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data  = info.global("appl_data").getr<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;
    const auto &e_proj_state = info("state", "proj_state").getr<ProjectState>();
    const auto &e_vert_spec  = info("gen_spectral_data", "spectra").getr<std::vector<Spec>>();

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

    // Push gamut data, given any state change
    ColrSystem csys = e_proj_data.csys(m_mapping_i);
    for (uint i = 0; i < e_proj_data.verts.size(); ++i) {
      guard_continue(m_is_mutated || e_proj_state.csys[m_mapping_i] || e_proj_state.verts[i]);
      m_vert_map[i] = csys.apply(e_vert_spec[i]);
      m_vert_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_unif_buffer);
    m_program.bind("b_vert", m_vert_buffer);
    m_program.bind("b_elem", info("gen_convex_weights", "elem_buffer").getr<gl::Buffer>());
    m_program.bind("b_bary", info("gen_convex_weights", "bary_buffer").getr<gl::Buffer>());
    m_program.bind("b_colr", info("gen_convex_weights", "colr_buffer").getr<gl::Buffer>());
    m_program.bind("i_colr", info("colr_texture").getw<TextureType>());

    // Dispatch shader to generate color-mapped buffer
    gl::dispatch_compute(m_dispatch);

    m_is_mutated = false;
  }

  void GenErrorMappingTask::set_texture_info(SchedulerHandle &info, TextureInfo texture_info) {
    met_trace_full();

    guard(!m_texture_info.size.isApprox(texture_info.size) || m_texture_info.levels != texture_info.levels);
    
    m_texture_info = texture_info;

    // Get external resources
    const auto &e_appl_data  = info.global("appl_data").getr<ApplicationData>();
    const auto &e_proj_data  = e_appl_data.project_data;

    // Create texture output for this task
    info("colr_texture").init<TextureType, TextureInfo>(m_texture_info);

    // Update texture info in uniform buffer
    m_unif_map->size_out = m_texture_info.size;
    m_unif_buffer.flush();
    
    // Rebuild dispatch object
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      const uint dispatch_n = m_texture_info.size.prod();
      const uint dispatch_ndiv = ceil_div(dispatch_n, ceil_div(256u, 4u));
      m_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_program };
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const uint dispatch_n = m_texture_info.size.prod();
      const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);
      m_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_program };
    }

    m_is_mutated = true;
  }
} // namespace met