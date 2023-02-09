#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <implot.h>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto mapping_subtask_fmt  = FMT_COMPILE("gen_color_mapping_texture_{}");
  constexpr auto resample_fmt = FMT_COMPILE("mappings_viewer_resample_{}");

  namespace detail {
    // Lambda captures of texture_size parameter and outputs
    // capture to add a resample task
    constexpr auto resample_subtask_add = [](const eig::Array2u &texture_size) {
      return [=](detail::AbstractTaskInfo &, uint i) -> detail::TextureResampleTask<gl::Texture2d4f> {
        return {{ .input_key    = { fmt::format(mapping_subtask_fmt, i), "texture" },
                  .output_key   = { fmt::format(resample_fmt, i), "texture"        },
                  .texture_info = { .size = texture_size                           },
                  .sampler_info = { .min_filter = gl::SamplerMinFilter::eLinear,
                                    .mag_filter = gl::SamplerMagFilter::eLinear    },
                  .lrgb_to_srgb = true                                             }};
      };
    };

    // Lambda capture to remove a resample task
    constexpr auto resample_subtask_rmv = [](detail::AbstractTaskInfo &, uint i) {
      return fmt::format(resample_fmt, i);
    };
  } // namespace detail

  void MappingsViewerTask::eval_tooltip_copy(detail::TaskEvalInfo &info, uint texture_i) {
    met_trace_full();

    // Get shared resources
    auto &e_bary_buffer = info.get_resource<gl::Buffer>("gen_barycentric_weights", "bary_buffer");
    auto &e_tex_data    = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                           - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                           / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    m_tooltip_pixel = (mouse_pos * e_tex_data.size().cast<float>()).cast<int>();
    const size_t sample_i = e_tex_data.size().x() * m_tooltip_pixel.y() + m_tooltip_pixel.x();

    // Perform copy of relevant barycentric data to current available buffer
    e_bary_buffer.copy_to(m_tooltip_buffers[m_tooltip_cycle_i], sizeof(Bary), sizeof(Bary) * sample_i);

    // Submit a fence for current available buffer as it affects mapped memory
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_tooltip_fences[m_tooltip_cycle_i] = gl::sync::Fence(gl::sync::time_s(1));
  }

  void MappingsViewerTask::eval_tooltip(detail::TaskEvalInfo &info, uint texture_i) {
    met_trace_full();

    // Get shared resources
    auto &e_appl_data  = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &e_proj_data  = e_appl_data.project_data;
    auto &e_gamut_spec = info.get_resource<std::vector<Spec>>("gen_spectral_gamut", "gamut_spec");

    // Spawn tooltip
    ImGui::BeginTooltip();
    ImGui::Text("Inspecting pixel (%i, %i)", m_tooltip_pixel.x(), m_tooltip_pixel.y());
    ImGui::Separator();

    // Acquire output barycentric data, which should by now be copied into the next buffer
    // Check fence for this buffer, however, in case this is not the case
    m_tooltip_cycle_i = (m_tooltip_cycle_i + 1) % m_tooltip_buffers.size();
    if (auto &fence = m_tooltip_fences[m_tooltip_cycle_i]; fence.is_init())
      fence.cpu_wait();

    // Compute output reflectance
    ColrSystem mapp  = e_proj_data.csys(texture_i);
    Bary bary        = m_tooltip_maps[m_tooltip_cycle_i][0];
    bary /= bary.sum(); // Normalize here, instead of fetching data for this from a gpu buffer
    Spec reflectance = 0;
    for (uint i = 0; i < e_gamut_spec.size(); ++i)
      reflectance += bary[i] * e_gamut_spec[i];
    Spec power       = mapp.illuminant * reflectance;
    Colr color       = mapp(reflectance);

    ImGui::PlotLines("Reflectance", reflectance.data(), wavelength_samples, 0,
      nullptr, 0.f, 1.f, { 0.f, 64.f });
    ImGui::PlotLines("Power", power.data(), wavelength_samples, 0,
      nullptr, 0.f, mapp.illuminant.maxCoeff(), { 0.f, 64.f });
    ImGui::PlotLines("Weights", bary.data(), barycentric_weights, 0,
      nullptr, 0.f, 1.f, { 0.f, 64.f });
    ImGui::ColorEdit3("Color (sRGB)", lrgb_to_srgb(color).data(), ImGuiColorEditFlags_Float);

    ImGui::Separator();
    
    ImGui::Value("Minimum", reflectance.minCoeff(), "%.6f");
    ImGui::Value("Maximum", reflectance.maxCoeff(), "%.6f");
    ImGui::Value("Bounded", reflectance.minCoeff() >= 0.f && reflectance.maxCoeff() <= 1.f);

    ImGui::EndTooltip();
  }

  void MappingsViewerTask::eval_popout(detail::TaskEvalInfo &info, uint texture_i) {
    // ...
  }

  void MappingsViewerTask::eval_save(detail::TaskEvalInfo &info, uint texture_i) {
    if (fs::path path; detail::save_dialog(path, "bmp")) {
      // Get shared resources
      auto color_task_key = fmt::format("gen_color_mapping_{}", texture_i);
      auto &e_colr_buffer = info.get_resource<gl::Buffer>(color_task_key, "colr_buffer");
      auto &e_appl_data   = info.get_resource<ApplicationData>(global_key, "app_data");

      // Obtain cpu-side texture
      Texture2d3f_al texture_al = {{ .size = e_appl_data.loaded_texture.size() }};
      e_colr_buffer.get(cast_span<std::byte>(texture_al.data()));

      // Remove padding bytes and apply gamma correction, then save to disk
      Texture2d3f texture = io::as_srgb(io::as_unaligned(texture_al));
      io::save_texture2d(io::path_with_ext(path, "bmp"), texture);
    }
  }

  MappingsViewerTask::MappingsViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void MappingsViewerTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    m_resample_size   = 1;
    m_tooltip_cycle_i = 0;

    // Initialize a set of rolling buffers of size Spec, and map these for reading
    constexpr auto create_flags = gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapRead;
    constexpr auto map_flags    = gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapRead;
    for (uint i = 0; i < m_tooltip_buffers.size(); ++i) {
      auto &buffer = m_tooltip_buffers[i];
      auto &map    = m_tooltip_maps[i];

      buffer = {{ .size = sizeof(Bary), .flags = create_flags }};
      map = cast_span<Bary>(buffer.map(map_flags));
    }
  }
  
  void MappingsViewerTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
    for (auto &buffer : m_tooltip_buffers) {
      buffer.unmap();
    }
    m_resample_tasks.dstr(info);
  }

  void MappingsViewerTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();
    
    if (ImGui::Begin("Mappings viewer")) {
      // Get shared resources
      auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_proj_data = e_appl_data.project_data;
      uint e_mappings_n = e_proj_data.color_systems.size();

      // Set up drawing a nr. of textures in a column-based layout; determine texture res.
      uint n_cols = 2;
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      eig::Array2f texture_size = viewport_size
                                 * e_appl_data.loaded_texture.size().cast<float>().y()
                                 / e_appl_data.loaded_texture.size().cast<float>().x()
                                 * 0.95f / static_cast<float>(n_cols);
                                 
      // If texture size has changed, respawn texture resample tasks
      if (auto resample_size = texture_size.max(1.f).cast<uint>(); !resample_size.isApprox(m_resample_size)) {
        // Reinitialize resample subtasks on texture size change
        m_resample_size = resample_size;
        m_resample_tasks.init(name(), info, e_mappings_n, 
          detail::resample_subtask_add(m_resample_size), detail::resample_subtask_rmv);
      } else {
        // Adjust nr. of spawned tasks to correct number
        m_resample_tasks.eval(info, e_mappings_n);
      }

      // Reset state for tooltip
      m_tooltip_mapping_i = -1;
      
      // Iterate n_cols, n_rows, and n_mappings
      for (uint i = 0, i_col = 0; i < e_mappings_n; ++i) {
        ImGui::PushID(fmt::format("mapping_viewer_texture_{}", i).c_str());

        // Generate name of task holding texture data
        auto subtask_tex_key = fmt::format(resample_fmt, i);
        
        // Get externally shared resources; note, resources may not be created yet as tasks are
        // added into the schedule at the end of a loop, not during
        guard_continue(info.has_resource(subtask_tex_key, "texture"));
        auto &e_texture = info.get_resource<gl::Texture2d4f>(subtask_tex_key, "texture");

        // Draw image
        ImGui::BeginGroup();

        // Header line
        ImGui::SetNextItemWidth(texture_size.x() * 0.6);
        ImGui::Text(e_proj_data.csys_name(i).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Export")) eval_save(info, i);
        
        // Main image
        ImGui::Image(ImGui::to_ptr(e_texture.object()), texture_size);
        
        // Set id for tooltip after loop is over, and start data copy
        if (ImGui::IsItemHovered()) {
          m_tooltip_mapping_i = i;
          eval_tooltip_copy(info, m_tooltip_mapping_i); 
        }

        // Do pop-out if image is double clicked
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { eval_popout(info, i); }
        
        ImGui::EndGroup();
        
        // Increment column count; insert same-line if on same row and not last item
        i_col++;
        if (i_col >= n_cols) {
          i_col = 0;
        } else if (i < e_mappings_n - 1) {
          ImGui::SameLine();
        }

        ImGui::PopID();
      }

      // Handle tooltip after data copy is hopefully completed
      if (m_tooltip_mapping_i != -1) {
        eval_tooltip(info, m_tooltip_mapping_i);
      }
    }
    ImGui::End();
  }
} // namespace met