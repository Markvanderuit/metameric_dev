#include <metameric/core/data.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <metameric/components/views/detail/panscan.hpp>
#include <metameric/components/views/embedding_viewport/task_draw_embedding.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/utility.hpp>
#include <algorithm>
#include <ranges>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u * 2048u; // up to 1024 verts across 1024 constraint sets?

  void ViewportDrawEmbeddingTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;

    // Set up gamut buffer and establish a flushable mapping
    m_vert_buffer = {{ .size = buffer_init_size * sizeof(AlColr), .flags = buffer_create_flags }};
    m_vert_map    = m_vert_buffer.map_as<AlColr>(buffer_access_flags);

    // Set up uniform buffer and establish a flushable mapping
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_unif_map->size_in = e_appl_data.loaded_texture.size();

    // Initialize program object
    m_program = {{ .type       = gl::ShaderType::eVertex,   
                   .spirv_path = "resources/shaders/views/draw_embedding.vert.spv",
                   .cross_path = "resources/shaders/views/draw_embedding.vert.json" },
                 { .type       = gl::ShaderType::eFragment, 
                   .spirv_path = "resources/shaders/views/draw_embedding.frag.spv",
                   .cross_path = "resources/shaders/views/draw_embedding.frag.json" }};

    // Generate 128x128 hardcoded positions
    std::vector<eig::Array2f> buffer_data;
    for (uint j = 0; j < 256; ++j) {
      for (uint i = 0; i < 256; ++i) {
        buffer_data.push_back({ i, j });
      }
    }
    m_data_buffer = {{ .data = cnt_span<const std::byte>(buffer_data) }};

    // Initialize dispatch object
    m_array = {{ }};
    m_draw = { .type             = gl::PrimitiveType::eTriangles,
               .draw_op          = gl::DrawOp::eFill,
               .bindable_array   = &m_array,
               .bindable_program = &m_program };
  }

  bool ViewportDrawEmbeddingTask::is_active(SchedulerHandle &info) {
    met_trace();
    return info("gen_random_constraints", "constraints").is_init() &&
           info.global("appl_data").read_only<ApplicationData>().project_data.color_systems.size() > 1; 
  }

  void ViewportDrawEmbeddingTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_panscan     = info.relative("view_input")("panscan").read_only<detail::Panscan>();
    const auto &e_lrgb_target = info.relative("view_begin")("lrgb_target").read_only<gl::Texture2d4f>();
    const auto &e_delaunay    = info("gen_convex_weights", "delaunay").read_only<AlignedDelaunayData>();
    const auto &e_vert_slct   = info("viewport.input.vert", "selection").read_only<std::vector<uint>>();
    const auto &e_vert_spec   = info("gen_spectral_data", "spectra").read_only<std::vector<Spec>>();
    const auto &e_constraints = info("gen_random_constraints", "constraints").read_only<
      std::vector<std::vector<ProjectData::Vert>>
    >();

    // Update uniform data
    eig::Array2f viewport_size = e_lrgb_target.size().cast<float>();
    m_unif_map->camera_matrix = e_panscan.full().matrix();
    m_unif_map->viewport_aspect = { 1.f, viewport_size.x() / viewport_size.y() };
    m_unif_map->n_verts = e_delaunay.verts.size();
    m_unif_map->n_quads = e_constraints.size();
    m_unif_buffer.flush();

    // Update draw data
    m_draw.vertex_count = 3 * static_cast<uint>(e_constraints.size());

    // Update per-constraint vertex data on constraint update
    if (info("gen_random_constraints", "constraints").is_mutated()) {
      // Obtain differense of all vertex indices and selected vertex indices
      auto vert_diff = std::views::iota(0u, static_cast<uint>(e_delaunay.verts.size()))
                     | std::views::filter([&](uint i) { return std::ranges::find(e_vert_slct, i) == e_vert_slct.end(); });

      ColrSystem mapping_csys = e_proj_data.csys(m_mapping_i);

      #pragma omp parallel for
      for (int j = 0; j < e_constraints.size(); ++j) {
        const auto &e_verts = e_constraints[j];
        for (uint i : vert_diff)
          m_vert_map[j * e_delaunay.verts.size() + i] = mapping_csys.apply_color_direct(e_vert_spec[i]);
        for (uint i : e_vert_slct)
          m_vert_map[j * e_delaunay.verts.size() + i] = e_verts[i].colr_j[0];
      } // for j

      m_vert_buffer.flush(sizeof(AlColr) * e_constraints.size() * e_delaunay.verts.size(), 0);
    }

    // Set local state
    gl::state::ScopedSet(gl::DrawCapability::eCullOp,     false);
    gl::state::ScopedSet(gl::DrawCapability::eDepthClamp, false);

    // Bind required buffers to corresponding targets
    m_program.bind("b_unif", m_unif_buffer);
    m_program.bind("b_data", m_data_buffer);
    m_program.bind("b_vert", m_vert_buffer);
    m_program.bind("b_bary", info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>());
    m_program.bind("b_elem", info("gen_convex_weights", "elem_buffer").read_only<gl::Buffer>());

    // Dispatch shader to draw color-mapped quads
    gl::dispatch_draw(m_draw);
  }
} // namespace met