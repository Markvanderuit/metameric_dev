#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_weight_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWritePersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWritePersistent | gl::BufferAccessFlags::eMapFlush;
  constexpr uint buffer_init_size    = 1024u;

  void WeightViewerTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;

    // Initialize objects for shader call
    const uint dispatch_n = e_appl_data.loaded_texture.size().prod();
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      const uint dispatch_ndiv = ceil_div(dispatch_n, 256u / generalized_weights);
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .spirv_path = "resources/shaders/views/draw_weights_generalized.comp.spv",
                     .cross_path = "resources/shaders/views/draw_weights_generalized.comp.json" }};
      m_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_program }; 
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);
      m_program = {{ .type = gl::ShaderType::eCompute,
                     .spirv_path = "resources/shaders/views/draw_weights_delaunay.comp.spv",
                     .cross_path = "resources/shaders/views/draw_weights_delaunay.comp.json" }};
      m_dispatch = { .groups_x = dispatch_ndiv, .bindable_program = &m_program }; 
    }

    // Initialize relevant buffers and writeable, flushable mapping
    m_vert_buffer = {{ .size = buffer_init_size * sizeof(AlColr), .flags = buffer_create_flags }};
    m_vert_map    = m_vert_buffer.map_as<AlColr>(buffer_access_flags);
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map    = m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags).data();
    m_unif_map->n = e_appl_data.loaded_texture.size().prod();

    // Initialize buffer object for storing intermediate results
    info("colr_buffer").init<gl::Buffer>({ .size = sizeof(AlColr) * dispatch_n });

    // Create subtask to handle buffer->texture copy and lrgb->srgb conversion
    TextureSubtask texture_subtask = {{ .input_key    = { info.task().key(), "colr_buffer" }, 
                                        .output_key   = "colr_texture",
                                        .texture_info = { .size = e_appl_data.loaded_texture.size() },
                                        .lrgb_to_srgb = true }};
    info.subtask("gen_texture").set(std::move(texture_subtask));
  }

  void WeightViewerTask::eval(SchedulerHandle &info) {
    met_trace_full();

    if (ImGui::Begin("Weight viewer")) {
      // Get external resources 
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_txtr_data = e_appl_data.loaded_texture;

      // Weight data is drawn to buffer in this function
      eval_draw(info);

      // Determine texture size in view
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      float texture_aspect       = static_cast<float>(e_txtr_data.size()[1]) / static_cast<float>(e_txtr_data.size()[0]);
      eig::Array2f texture_size  = (viewport_size * texture_aspect).max(1.f).eval();

      // Display ImGui components
      auto subtask_name = fmt::format("{}.gen_texture", info.task().key());
      if (auto rsrc = info(subtask_name, "colr_texture"); rsrc.is_init()) {
        ImGui::Image(ImGui::to_ptr(rsrc.read_only<gl::Texture2d4f>().object()), texture_size);
      }
    }
    ImGui::End();
  }

  void WeightViewerTask::eval_draw(SchedulerHandle &info) {
    met_trace_full();

    // Continue only on relevant state changes
    const auto &e_proj_state = info("state", "proj_state").read_only<ProjectState>();
    const auto &e_view_state = info("state", "view_state").read_only<ViewportState>();
    bool activate_flag = e_proj_state.verts || e_view_state.vert_selection || e_view_state.cstr_selection;
    guard(activate_flag);

    // Continue only if vertex selection is non-empty
    // otherwise, blacken output texture and return
    const auto &e_selection = info("viewport.input.vert", "selection").read_only<std::vector<uint>>();
    if (e_selection.empty()) {
      info("colr_buffer").writeable<gl::Buffer>().clear();
      return;
    }

    // Get external resources 
    const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_cstr_slct   = info("viewport.overlay", "constr_selection").read_only<int>();
    const auto &e_bary_buffer = info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>();
    const auto &e_elem_buffer = info("gen_convex_weights", "elem_buffer").read_only<gl::Buffer>();
    const auto &e_vert_spec   = info("gen_spectral_data", "spectra").read_only<std::vector<Spec>>();

    // Get modified resources 
    auto &i_colr_buffer = info("colr_buffer").writeable<gl::Buffer>();

    // Index of selected mapping is used for color queries
    uint mapping_i = e_cstr_slct >= 0 ? e_proj_data.verts[e_selection[0]].csys_j[e_cstr_slct] : 0;

    // Update uniform data
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      m_unif_map->n_verts = e_proj_data.verts.size();
      m_unif_map->n_elems = e_proj_data.elems.size();
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const auto e_delaunay = info("gen_convex_weights", "delaunay").read_only<AlignedDelaunayData>();
      m_unif_map->n_verts = e_delaunay.verts.size();
      m_unif_map->n_elems = e_delaunay.elems.size();
    }
    m_unif_map->n = e_appl_data.loaded_texture.size().prod();
    std::fill(m_unif_map->selection, m_unif_map->selection + e_proj_data.verts.size(), 0);
    std::ranges::for_each(e_selection, [&](uint i) { m_unif_map->selection[i] = 1; });
    m_unif_buffer.flush();

    // Update vertex data, given any state change
    ColrSystem csys = e_proj_data.csys(mapping_i);
    for (uint i = 0; i < e_proj_data.verts.size(); ++i) {
      guard_continue(activate_flag || e_proj_state.verts[i]);
      m_vert_map[i] = csys.apply_color_indirect(e_vert_spec[i]);
      m_vert_buffer.flush(sizeof(AlColr), i * sizeof(AlColr));
    }

    // Bind resources
    m_program.bind("b_unif", m_unif_buffer);
    m_program.bind("b_bary", e_bary_buffer);
    m_program.bind("b_vert", m_vert_buffer);
    m_program.bind("b_elem", e_elem_buffer);
    m_program.bind("b_colr", i_colr_buffer);

    // Dispatch shader to compute weight texture data
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch);
  }
} // namespace met