#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_error_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>

namespace met {
  constexpr auto texture_fmt  = FMT_COMPILE("{}_gen_texture");
  constexpr auto resample_fmt = FMT_COMPILE("{}_gen_resample");
  constexpr auto mapping_fmt  = FMT_COMPILE("gen_color_mapping_{}");

  void ErrorViewerTask::eval_tooltip_copy(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_tex_data     = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;
    auto &e_color_input  = info.get_resource<gl::Buffer>("gen_spectral_texture", "color_buffer");
    auto &e_color_output = info.get_resource<gl::Buffer>(fmt::format(mapping_fmt, m_mapping_i), "color_buffer");
    auto &i_color_error  = info.get_resource<gl::Buffer>("color_buffer");

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                           - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                           / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    m_tooltip_pixel = (mouse_pos * e_tex_data.size().cast<float>()).cast<int>();
    const size_t sample_i = e_tex_data.size().x() * m_tooltip_pixel.y() + m_tooltip_pixel.x();

    // Perform copy of relevant reflectance data to current available buffers
    e_color_input.copy_to(m_tooltip_buffers[m_tooltip_cycle_i].in_a, sizeof(AlColr), sizeof(AlColr) * sample_i);
    e_color_output.copy_to(m_tooltip_buffers[m_tooltip_cycle_i].in_b, sizeof(AlColr), sizeof(AlColr) * sample_i);
    i_color_error.copy_to(m_tooltip_buffers[m_tooltip_cycle_i].out, sizeof(AlColr), sizeof(AlColr) * sample_i);

    // Submit a fence for current available buffer as it affects mapped memory
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_tooltip_fences[m_tooltip_cycle_i] = gl::sync::Fence(gl::sync::time_s(1));
  } 

  void ErrorViewerTask::eval_tooltip(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Spawn tooltip
    ImGui::BeginTooltip();
    ImGui::Text("Inspecting pixel (%i, %i)", m_tooltip_pixel.x(), m_tooltip_pixel.y());
    ImGui::Separator();

    // Acquire output error data, which should by now be copied into the a buffer
    // Check fence for this buffer, however, in case this is not the case
    m_tooltip_cycle_i = (m_tooltip_cycle_i + 1) % m_tooltip_buffers.size();
    if (auto &fence = m_tooltip_fences[m_tooltip_cycle_i]; fence.is_init()) {
      fence.cpu_wait();
    }
    auto &color_maps = m_tooltip_maps[m_tooltip_cycle_i];

    // Plot rest of tooltip
    ImGui::ColorEdit3("Input", color_maps.in_a[0].data(), ImGuiColorEditFlags_Float);
    ImGui::ColorEdit3("Mapping", color_maps.in_b[0].data(), ImGuiColorEditFlags_Float);
    ImGui::Separator();
    ImGui::ColorEdit3("Error (abs)", color_maps.out[0].data(), ImGuiColorEditFlags_Float);
    ImGui::EndTooltip();
  }

  void ErrorViewerTask::eval_error(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_color_input  = info.get_resource<gl::Buffer>("gen_spectral_texture", "color_buffer");
    auto &e_color_output = info.get_resource<gl::Buffer>(fmt::format(mapping_fmt, m_mapping_i), "color_buffer");
    auto &i_color_error  = info.get_resource<gl::Buffer>("color_buffer");

    // Bind resources to buffer targets
    e_color_input.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    e_color_output.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_color_error.bind_to(gl::BufferTargetType::eShaderStorage,  2);

    // Dispatch shader to generate spectral dataw
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_error_dispatch);
  }

  ErrorViewerTask::ErrorViewerTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void ErrorViewerTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    m_resample_size = 1;
    m_mapping_i     = 0;

    // Initialize a set of rolling buffers for the tooltip, and map these for reading
    constexpr auto create_flags = gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapRead;
    constexpr auto map_flags    = gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapRead;
    for (uint i = 0; i < m_tooltip_buffers.size(); ++i) {
      auto &buffer_obj = m_tooltip_buffers[i];
      auto &map_obj    = m_tooltip_maps[i];

      buffer_obj = { .in_a = {{ .size = sizeof(AlColr), .flags = create_flags }},
                     .in_b = {{ .size = sizeof(AlColr), .flags = create_flags }},
                     .out  = {{ .size = sizeof(AlColr), .flags = create_flags }}};
      map_obj = { .in_a = cast_span<AlColr>(buffer_obj.in_a.map(map_flags)),
                  .in_b = cast_span<AlColr>(buffer_obj.in_b.map(map_flags)),
                  .out  = cast_span<AlColr>(buffer_obj.out.map(map_flags)) };
    }
    m_tooltip_cycle_i = 0;

    // Get externally shared resources
    auto &e_tex_data = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;

    // Initialize error computation components
    const uint generate_n    = e_tex_data.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);
    m_error_program = {{ .type = gl::ShaderType::eCompute,
                         .path = "resources/shaders/misc/buffer_error.comp" }};
    m_error_dispatch = { .groups_x = generate_ndiv, 
                         .bindable_program = &m_error_program };
    
    // Set these uniforms once
    m_error_program.uniform("u_n", generate_n);

    // Insert buffer object to hold error data
    info.emplace_resource<gl::Buffer>("color_buffer", { .size = generate_n * sizeof(AlColr) });

    // Insert subtask to handle buffer->texture conversion
    TextureSubtask subtask({ name(), "color_buffer" },
                           { fmt::format(texture_fmt, name()), "texture" },
                           { .size = e_tex_data.size() });
    info.insert_task_after(name(), std::move(subtask));
  }

  void ErrorViewerTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();
    for (auto &buffer_obj : m_tooltip_buffers) {
      buffer_obj.in_a.unmap();
      buffer_obj.in_b.unmap();
      buffer_obj.out.unmap();
    }
  }

  void ErrorViewerTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    if (ImGui::Begin("Error viewer")) {
      // Get shared resources
      auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_tex_data = e_app_data.loaded_texture;
      auto &e_prj_data = e_app_data.project_data;
      auto &e_mappings = e_prj_data.mappings;

      // Get subtask names
      auto texture_subtask_name  = fmt::format(texture_fmt, name());
      auto resample_subtask_name = fmt::format(resample_fmt, name());

      // Local state
      bool handle_toolip = false;
      
      // 0. Introduce settings
      if (ImGui::BeginCombo("Selected mapping", e_mappings[m_mapping_i].first.c_str())) {
        for (uint i = 0; i < e_mappings.size(); ++i) {
          if (ImGui::Selectable(e_mappings[i].first.c_str(), i == m_mapping_i)) {
            m_mapping_i = i;
          }
        }
        ImGui::EndCombo();
      }

      // 1. Handle error computation
      eval_error(info);

      // 2. Handle texture resampling subtask
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      eig::Array2f texture_size = viewport_size * e_tex_data.size().y() / e_tex_data.size().x()
                                * 0.95f;;

      if (auto resample_size = texture_size.cast<uint>().max(1u); !resample_size.isApprox(m_resample_size)) {
        m_resample_size = resample_size;

        // Remove previous resample subtask and insert a new one
        ResampleSubtask subtask({ texture_subtask_name, "texture"             },
                                { resample_subtask_name, "texture"            },
                                { .size = resample_size                       },
                                { .min_filter = gl::SamplerMinFilter::eLinear ,
                                  .mag_filter = gl::SamplerMagFilter::eLinear });
        info.remove_task(resample_subtask_name);
        info.insert_task_after(texture_subtask_name, std::move(subtask));
      }

      // 3. Display ImGui components to show error and select mapping
      if (info.has_resource(resample_subtask_name, "texture")) {
        auto &e_texture = info.get_resource<gl::Texture2d4f>(resample_subtask_name, "texture");

        ImGui::Image(ImGui::to_ptr(e_texture.object()), texture_size);

        // 4. Signal tooltip and start data copy
        if (ImGui::IsItemHovered()) {
          handle_toolip = true;
          eval_tooltip_copy(info);
        }
      }

      // 5. Handle tooltip after data copy is hopefully completed
      if (handle_toolip) {
        eval_tooltip(info);
      }
    }
    ImGui::End();
  }
} // namespace met