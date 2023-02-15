#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_weight_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>

namespace met {
  constexpr auto sub_texture_fmt  = FMT_COMPILE("{}_gen_texture");
  constexpr auto sub_resample_fmt = FMT_COMPILE("{}_gen_resample");
  constexpr auto buffer_create_flags = gl::BufferCreateFlags::eMapWrite | gl::BufferCreateFlags::eMapPersistent;
  constexpr auto buffer_access_flags = gl::BufferAccessFlags::eMapWrite | gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapFlush;

  WeightViewerTask::WeightViewerTask(const std::string &name)
  : detail::AbstractTask(name, false) { }

  void WeightViewerTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture_f32;

    // Nr. of workgroups for sum computation
    const uint dispatch_n    = e_texture.size().prod();
    const uint dispatch_ndiv = ceil_div(dispatch_n, 256u / barycentric_weights);

    // Initialize objects for shader call
    m_program = {{ .type = gl::ShaderType::eCompute,
                   .path = "resources/shaders/viewport/draw_weights.comp.spv_opt",
                   .is_spirv_binary = true }};
    m_dispatch = { .groups_x = dispatch_ndiv, 
                   .bindable_program = &m_program }; 

    // Initialize uniform buffer and writeable, flushable mapping
    m_unif_buffer = {{ .size = sizeof(UniformBuffer), .flags = buffer_create_flags }};
    m_unif_map = &m_unif_buffer.map_as<UniformBuffer>(buffer_access_flags)[0];

    // Initialize buffer object for storing intermediate results
    info.emplace_resource<gl::Buffer>("colr_buffer", { .size = sizeof(AlColr) * dispatch_n });

    // Insert subtask to handle buffer->texture and lrgb->srgb conversion
    TextureSubtask task = {{ .input_key  = { name(), "colr_buffer" },
                             .output_key = { fmt::format(sub_texture_fmt, name()), "colr_texture" },
                             .texture_info = { .size = e_texture.size() }}};
    info.insert_task_after(name(), std::move(task));
  }
  
  void WeightViewerTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    info.remove_task(fmt::format(sub_texture_fmt, name()));
    info.remove_task(fmt::format(sub_resample_fmt, name()));
  }

  void WeightViewerTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    if (ImGui::Begin("Weight viewer")) {
      // Get shared resources 
      auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_txtr_data   = e_appl_data.loaded_texture_f32;

      // Weight data is drawn to texture in this function
      eval_draw(info);

      // Determine texture size
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      float texture_aspect = static_cast<float>(e_txtr_data.size()[1]) / static_cast<float>(e_txtr_data.size()[0]);
      auto texture_size    = (viewport_size * texture_aspect).max(1.f).cast<uint>().eval();

      // Check if the resample subtask needs readjusting for a resized output texture
      if (!texture_size.isApprox(m_texture_size)) {
        m_texture_size = texture_size;

        // Define new resample subtask
        ResampleSubtask task = {{ .input_key  = { fmt::format(sub_texture_fmt, name()), "colr_texture" },
                                  .output_key = { fmt::format(sub_resample_fmt, name()), "colr_texture" },
                                  .texture_info = { .size = m_texture_size },
                                  .sampler_info = { .min_filter = gl::SamplerMinFilter::eLinear,
                                                    .mag_filter = gl::SamplerMagFilter::eLinear },
                                  .lrgb_to_srgb = true}};
        
        // Replace task; this is safe if the task does not yet exist
        info.remove_task(fmt::format(sub_resample_fmt, name()));
        info.insert_task_after(fmt::format(sub_texture_fmt, name()), std::move(task));
      }

      // View data is defined in this function
      eval_view(info);
    }
    ImGui::End();

  }

  void WeightViewerTask::eval_draw(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only on relevant state changes
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    auto &e_view_state = info.get_resource<ViewportState>("state", "viewport_state");
    bool activate_flag = e_pipe_state.any_verts || e_view_state.vert_selection || e_view_state.cstr_selection;
    info.get_resource<bool>(fmt::format(sub_texture_fmt, name()), "activate_flag") = activate_flag;
    guard(activate_flag);

    // Continue only if vertex selection is non-empty
    // otherwise, blacken output texture and return
    auto &e_selection = info.get_resource<std::vector<uint>>("viewport_input_vert", "selection");
    if (e_selection.empty()) {
      auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");
      i_colr_buffer.clear();
      return;
    }

    // Get shared resources 
    auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data   = e_appl_data.project_data;
    auto &e_cstr_slct   = info.get_resource<int>("viewport_overlay", "constr_selection");
    auto &e_bary_buffer = info.get_resource<gl::Buffer>("gen_barycentric_weights", "bary_buffer");
    uint mapping_index  = e_cstr_slct >= 0 ? e_proj_data.gamut_verts[e_selection[0]].csys_j[e_cstr_slct] : 0;
    auto &e_colr_buffer = info.get_resource<gl::Buffer>(fmt::format("gen_color_mapping_{}", mapping_index), "colr_buffer");
    auto &i_colr_buffer = info.get_resource<gl::Buffer>("colr_buffer");

    // Update uniform data for upcoming sum computation
    m_unif_map->n       = e_appl_data.loaded_texture_f32.size().prod();
    m_unif_map->n_verts = e_appl_data.project_data.gamut_verts.size();
    std::ranges::fill(m_unif_map->selection, eig::Array4u(0));
    std::ranges::for_each(e_selection, [&](uint i) { m_unif_map->selection[i] = 1; });
    m_unif_buffer.flush();

    // Bind resources to buffer targets for upcoming computation
    e_bary_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 0);
    e_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_colr_buffer.bind_to(gl::BufferTargetType::eShaderStorage, 2);
    m_unif_buffer.bind_to(gl::BufferTargetType::eUniform,       0);

    // Dispatch shader
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_dispatch);
  }

  void WeightViewerTask::eval_view(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Continue only if the necessary output texture exists; this may not be the case on first run
    auto st_name = fmt::format(sub_resample_fmt, name());
    guard(info.has_resource(st_name, "colr_texture"));

    // Get shared resources
    auto &e_texture = info.get_resource<gl::Texture2d4f>(st_name, "colr_texture");

    ImGui::Image(ImGui::to_ptr(e_texture.object()), m_texture_size.cast<float>().eval());
  }
} // namespace met