#include <metameric/core/state.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/mappings_viewer/task_mapping_popout.hpp>
#include <small_gl/buffer.hpp>

namespace met {
  constexpr auto gen_subtask_fmt      = FMT_COMPILE("gen_color_mapping_{}");
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

  void MappingsViewerTask::handle_tooltip(detail::TaskEvalInfo &info, uint texture_i) {
    met_trace();

    // Get shared resources
    auto &e_spectrum_buffer = info.get_resource<gl::Buffer>("gen_spectral_texture", "spectrum_buffer");
    auto &e_app_data        = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_tex_data        = e_app_data.loaded_texture;
    auto &e_mapping         = e_app_data.loaded_mappings[texture_i];

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos  =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                            - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                            / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    eig::Array2i sample_pos = (mouse_pos * e_tex_data.size().cast<float>()).cast<int>();
    const size_t sample_i   = e_tex_data.size().x() * sample_pos.y() + sample_pos.x();

    // Copy reflectance data at sample position from gpu buffer
    Spec reflectance = cast_span<Spec>(e_spectrum_buffer.map(gl::BufferAccessFlags::eMapRead,
      sizeof(Spec), sizeof(Spec) * sample_i))[0];
    e_spectrum_buffer.unmap();

    // Spec reflectance;// m_spectrum_buffer_map[sample_i];
    // e_spectrum_buffer.get(as_span<std::byte>(reflectance), sizeof(Spec), sizeof(Spec) * sample_i);

    // Compute output on the fly for said data
    Spec  power     = e_mapping.apply_power(reflectance);
    Color power_rgb = e_mapping.apply_color(reflectance);

    // Spawn a simple tooltip showing reflectance, power, rgb values, etc.
    ImGui::BeginTooltip();
    ImGui::Text("Inspecting pixel (%i, %i)", sample_pos.x(), sample_pos.y());
    ImGui::Separator();
    ImGui::PlotLines("Reflectance", reflectance.data(), wavelength_samples, 0,
      nullptr, 0.f, 1.f, { 0.f, 64.f });
    ImGui::PlotLines("Power", power.data(), wavelength_samples, 0,
      nullptr, 0.f, power.maxCoeff(), { 0.f, 64.f });
    ImGui::ColorEdit3("Power (rgb)", power_rgb.data(), ImGuiColorEditFlags_Float);
    ImGui::Separator();
    ImGui::Text("Hint: double-click image to show it in a window");
    ImGui::EndTooltip();
  }

  void MappingsViewerTask::handle_popout(detail::TaskEvalInfo &info, uint texture_i) {
    // ...
  }

  MappingsViewerTask::MappingsViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void MappingsViewerTask::dstr(detail::TaskDstrInfo &info) {
    met_trace();
    
    m_resample_subtasks.dstr(info);
  }

  void MappingsViewerTask::eval(detail::TaskEvalInfo &info) {
    met_trace();
    
    if (ImGui::Begin("Mappings viewer")) {
      // Get shared resources
      auto &e_spectrum_buffer  = info.get_resource<gl::Buffer>("gen_spectral_texture", "spectrum_buffer");
      auto &e_app_data         = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_tex_data         = e_app_data.loaded_texture;
      auto &e_prj_data         = e_app_data.project_data;
      auto &e_mappings         = e_prj_data.mappings;
      uint e_mappings_n        = e_app_data.loaded_mappings.size();

      // Set up drawing a nr. of textures in a column-based layout; determine texture res.
      uint n_cols = 2;
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      eig::Array2f texture_scale = viewport_size
                                 * e_app_data.loaded_texture.size().y()
                                 / e_app_data.loaded_texture.size().x()
                                 * 0.95f / static_cast<float>(n_cols);
                                 
      // If texture size has changed, respawn texture resample tasks
      if (auto texture_size = texture_scale.cast<uint>(); !m_texture_size.isApprox(texture_size)) {
        // Reinitialize resample subtasks on texture size change
        m_texture_size = texture_size;
        m_resample_subtasks.init(name(), info, e_mappings_n, 
          resample_subtask_add(m_texture_size), resample_subtask_rmv);
      } else {
        // Adjust nr. of spawned tasks to correct number
        m_resample_subtasks.eval(info, e_mappings_n);
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
        ImGui::Image(ImGui::to_ptr(e_texture.object()), texture_scale);
        // ImGui::ImageButton(ImGui::to_ptr(e_texture.object()), texture_scale,
          // { 0, 0 }, { 1, 1 }, -1, { 0, 0, 0, 0 }, { 0.95, 1, 1, 1 });
        
        // Do pop-out if image is double clicked
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { handle_popout(info, i); }
        
        if (ImGui::IsItemHovered()) {
          // Mouse enters hover state; acquire map over spectral buffer
          // if (!e_spectrum_buffer.is_mapped()) {
          //   auto map_flags = gl::BufferAccessFlags::eMapRead | gl::BufferAccessFlags::eMapPersistent;
          //   m_spectrum_buffer_map = cast_span<Spec>(e_spectrum_buffer.map(map_flags));
          //   fmt::print("Mapping buffer");
          // }

          // Show tooltip on mouse-over
          handle_tooltip(info, i);
        } else {
          // Mouse no longer in hover state; release map over spectral buffer
          // if (e_spectrum_buffer.is_mapped()) {
          //   e_spectrum_buffer.unmap();
          //   m_spectrum_buffer_map = { };
          //   fmt::print("Unmapping buffer");
          // }
        }

        ImGui::EndGroup();
        
        // Increment column count; insert same-line if on same row and not last item
        i_col++;
        if (i_col >= n_cols) {
          i_col = 0;
        } else if (i < e_mappings.size() - 1) {
          ImGui::SameLine();
        }
      }
    }
    ImGui::End();
  }
} // namespace met