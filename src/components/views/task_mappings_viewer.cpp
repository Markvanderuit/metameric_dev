#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/mappings_viewer/task_mapping_popout.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  constexpr auto gen_subtask_tex_fmt  = FMT_COMPILE("gen_color_mapping_texture_{}");
  constexpr auto resample_subtask_fmt = FMT_COMPILE("mappings_viewer_resample_{}");

  // Lambda captures of texture_size parameter and outputs
  // capture to add a resample task
  constexpr auto resample_subtask_add = [](const eig::Array2u &texture_size) {
    return [=](detail::AbstractTaskInfo &, uint i) {
      using ResampleTaskType = detail::TextureResampleTask<gl::Texture2d4f>;
      return ResampleTaskType({ fmt::format(gen_subtask_tex_fmt, i), "texture"  }, 
                              { fmt::format(resample_subtask_fmt, i), "texture" },
                              { .size = texture_size                            }, 
                              { .min_filter = gl::SamplerMinFilter::eLinear,
                                .mag_filter = gl::SamplerMagFilter::eLinear     });
    };
  };

  // Lambda capture to remove a resample task
  constexpr auto resample_subtask_rmv = [](detail::AbstractTaskInfo &, uint i) {
    return fmt::format(resample_subtask_fmt, i);
  };

  void MappingsViewerTask::eval_tooltip_copy(detail::TaskEvalInfo &info, uint texture_i) {
    met_trace_full();

    // Get shared resources
    auto &e_spectrum_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "spectrum_buffer");
    auto &e_tex_data        = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos  =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                            - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                            / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    m_tooltip_pixel   = (mouse_pos * e_tex_data.size().cast<float>()).cast<int>();
    const size_t sample_i = e_tex_data.size().x() * m_tooltip_pixel.y() + m_tooltip_pixel.x();

    // Perform copy of relevant reflectance data to mapped buffer, then submit a fence
    e_spectrum_buffer.copy_to(m_tooltip_buffer, sizeof(Spec), sample_i * sizeof(Spec));
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_tooltip_fence = gl::sync::Fence(gl::sync::time_s(1));
  }

  void MappingsViewerTask::eval_tooltip(detail::TaskEvalInfo &info, uint texture_i) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_mapping  = e_app_data.loaded_mappings[texture_i];
    
    // Spawn tooltip
    ImGui::BeginTooltip();
    ImGui::Text("Inspecting pixel (%i, %i)", m_tooltip_pixel.x(), m_tooltip_pixel.y());
    ImGui::Separator();

    // Compute output for reflectance data once it is available; no choice but to wait here
    m_tooltip_fence.cpu_wait();
    Spec& reflectance = m_tooltip_map[0];
    Spec  power       = e_mapping.apply_power(reflectance);
    Color power_rgb   = e_mapping.apply_color(reflectance);
    
    // Plot rest of tooltip
    ImGui::PlotLines("Reflectance", reflectance.data(), wavelength_samples, 0,
      nullptr, 0.f, 1.f, { 0.f, 64.f });
    ImGui::PlotLines("Power", power.data(), wavelength_samples, 0,
      nullptr, 0.f, power.maxCoeff(), { 0.f, 64.f });
    ImGui::ColorEdit3("Power (rgb)", power_rgb.data(), ImGuiColorEditFlags_Float);
    ImGui::Separator();
    ImGui::Text("Hint: double-click image to show it in a window");
    ImGui::EndTooltip();
  }

  void MappingsViewerTask::eval_popout(detail::TaskEvalInfo &info, uint texture_i) {
    // ...
  }

  MappingsViewerTask::MappingsViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void MappingsViewerTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    m_resample_size = 1;

    // Initialize a buffer of size Spec, and map it for reading
    m_tooltip_buffer = {{ .size = sizeof(Spec),
                          .flags = gl::BufferCreateFlags::eMapPersistent
                                 | gl::BufferCreateFlags::eMapRead }};
    m_tooltip_map = cast_span<Spec>(m_tooltip_buffer.map(
      gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapRead));
  }
  
  void MappingsViewerTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
    
    m_resample_tasks.dstr(info);
  }

  void MappingsViewerTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    if (ImGui::Begin("Mappings viewer")) {
      // Get shared resources
      auto &e_app_data  = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_tex_data  = e_app_data.loaded_texture;
      auto &e_prj_data  = e_app_data.project_data;
      auto &e_mappings  = e_prj_data.mappings;
      uint e_mappings_n = e_app_data.loaded_mappings.size();

      // Set up drawing a nr. of textures in a column-based layout; determine texture res.
      uint n_cols = 2;
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      eig::Array2f texture_size = viewport_size
                                 * e_app_data.loaded_texture.size().y()
                                 / e_app_data.loaded_texture.size().x()
                                 * 0.95f / static_cast<float>(n_cols);
                                 
      // If texture size has changed, respawn texture resample tasks
      if (auto resample_size = texture_size.cast<uint>(); !resample_size.isApprox(m_resample_size)) {
        // Reinitialize resample subtasks on texture size change
        m_resample_size = resample_size;
        m_resample_tasks.init(name(), info, e_mappings_n, 
          resample_subtask_add(m_resample_size), resample_subtask_rmv);
      } else {
        // Adjust nr. of spawned tasks to correct number
        m_resample_tasks.eval(info, e_mappings_n);
      }

      // Iterate n_cols, n_rows, and n_mappings
      for (uint i = 0, i_col = 0; i < e_mappings.size(); ++i) {
        // Generate name of task holding texture data
        auto subtask_tex_key = fmt::format(resample_subtask_fmt, i);
        
        // Get externally shared resources; note, resources may not be created yet as tasks are
        // added into the schedule at the end of a loop, not during
        guard_continue(info.has_resource(subtask_tex_key, "texture"));
        auto &e_texture = info.get_resource<gl::Texture2d4f>(subtask_tex_key, "texture");

        // Draw image
        ImGui::BeginGroup();
        ImGui::Text(e_mappings[i].first.c_str());
        ImGui::Image(ImGui::to_ptr(e_texture.object()), texture_size);
        
        // Set id for tooltip after loop is over, and schedule copy of data
        if (m_tooltip_i = ImGui::IsItemHovered() ? i : -1; m_tooltip_i != -1) { 
          eval_tooltip_copy(info, m_tooltip_i); 
        }

        // Do pop-out if image is double clicked
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { eval_popout(info, i); }
        
        ImGui::EndGroup();
        
        // Increment column count; insert same-line if on same row and not last item
        i_col++;
        if (i_col >= n_cols) {
          i_col = 0;
        } else if (i < e_mappings.size() - 1) {
          ImGui::SameLine();
        }
      }

      // Handle tooltip after copy is hopefully completed
      if (m_tooltip_i != -1) { eval_tooltip(info, m_tooltip_i); }
    }
    ImGui::End();
  }
} // namespace met