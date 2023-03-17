#include <metameric/core/data.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_error_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

namespace met {
  constexpr float tooltip_width = 256.f;

  void ErrorViewerTask::eval_tooltip_copy(detail::SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    auto &e_txtr_data     = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture_f32;
    auto &e_color_input  = info.get_resource<gl::Buffer>("gen_delaunay_weights", "colr_buffer");
    auto &e_color_output = info.get_resource<gl::Buffer>("gen_color_mappings.gen_mapping_0", "colr_buffer");
    auto &i_color_error  = info.get_resource<gl::Buffer>("colr_buffer");

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                           - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                           / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    m_tooltip_pixel = (mouse_pos * e_txtr_data.size().cast<float>()).cast<int>();
    const size_t sample_i = e_txtr_data.size().x() * m_tooltip_pixel.y() + m_tooltip_pixel.x();

    // Perform copy of relevant reflectance data to current available buffers
    e_color_input.copy_to(m_tooltip_buffers[m_tooltip_cycle_i].in_a, sizeof(AlColr), sizeof(AlColr) * sample_i);
    e_color_output.copy_to(m_tooltip_buffers[m_tooltip_cycle_i].in_b, sizeof(AlColr), sizeof(AlColr) * sample_i);
    i_color_error.copy_to(m_tooltip_buffers[m_tooltip_cycle_i].out, sizeof(AlColr), sizeof(AlColr) * sample_i);

    // Submit a fence for current available buffer as it affects mapped memory
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_tooltip_fences[m_tooltip_cycle_i] = gl::sync::Fence(gl::sync::time_s(1));
  } 

  void ErrorViewerTask::eval_tooltip(detail::SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    auto &e_window = info.get_resource<gl::Window>(global_key, "window");

    // Spawn tooltip
    ImGui::BeginTooltip();
    ImGui::SetNextItemWidth(tooltip_width * e_window.content_scale());
    ImGui::Text("Inspecting pixel (%i, %i)", m_tooltip_pixel.x(), m_tooltip_pixel.y());
    ImGui::Separator();

    // Acquire output error data, which should by now be copied into the current buffer
    // Check fence for this buffer, however, in case this is not the case
    m_tooltip_cycle_i = (m_tooltip_cycle_i + 1) % m_tooltip_buffers.size();
    if (auto &fence = m_tooltip_fences[m_tooltip_cycle_i]; fence.is_init()) {
      fence.cpu_wait();
    }
    auto &color_maps = m_tooltip_maps[m_tooltip_cycle_i];

    // Plot tooltip values
    ImGui::ColorEdit3("Input color (lRGB)", color_maps.in_a[0].data(),  ImGuiColorEditFlags_Float);
    ImGui::ColorEdit3("Roundtrip color (lRGB)", color_maps.in_b[0].data(),  ImGuiColorEditFlags_Float);
    ImGui::ColorEdit3("Roundtrip error (abs)", color_maps.out[0].data(), ImGuiColorEditFlags_Float);
    
    ImGui::EndTooltip();
  }

  void ErrorViewerTask::eval_error(detail::SchedulerHandle &info) {
    // Continue only on relevant state changes
    auto &e_pipe_state = info.get_resource<ProjectState>("state", "pipeline_state");
    bool activate_flag = e_pipe_state.any;
    info.get_resource<bool>(fmt::format("{}.gen_texture", info.task_key()), "activate_flag") = activate_flag;
    guard(activate_flag);

    // Get shared resources
    auto &e_color_input  = info.get_resource<gl::Buffer>("gen_delaunay_weights", "colr_buffer");
    auto &e_color_output = info.get_resource<gl::Buffer>("gen_color_mappings.gen_mapping_0", "colr_buffer");
    auto &i_color_error  = info.get_resource<gl::Buffer>("colr_buffer");

    // Bind resources to buffer targets
    e_color_input.bind_to(gl::BufferTargetType::eShaderStorage,  0);
    e_color_output.bind_to(gl::BufferTargetType::eShaderStorage, 1);
    i_color_error.bind_to(gl::BufferTargetType::eShaderStorage,  2);

    // Dispatch shader to generate spectral dataw
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_error_dispatch);
  }

  void ErrorViewerTask::init(detail::SchedulerHandle &info) {
    met_trace_full();

    m_texture_size = 1;

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
    auto &e_txtr_data = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture_f32;

    // Initialize error computation components
    const uint generate_n    = e_txtr_data.size().prod();
    const uint generate_ndiv = ceil_div(generate_n, 256u);
    m_error_program = {{ .type = gl::ShaderType::eCompute,
                         .path = "resources/shaders/misc/buffer_error.comp" }};
    m_error_dispatch = { .groups_x = generate_ndiv, 
                         .bindable_program = &m_error_program };
    
    // Set these uniforms once
    m_error_program.uniform("u_n", generate_n);

    // Insert buffer object to hold error data
    info.emplace_resource<gl::Buffer>("colr_buffer", { .size = generate_n * sizeof(AlColr) });

    // Insert subtask to handle buffer->texture conversion
    TextureSubtask subtask = {{ .input_key    = { info.task_key(), "colr_buffer" },
                                .output_key   = { "blablabla", "colr_texture" },
                                .texture_info = { .size = e_txtr_data.size() }}};
    info.insert_subtask(info.task_key(), "gen_texture", std::move(subtask));
  }

  void ErrorViewerTask::dstr(detail::SchedulerHandle &info) {
    met_trace_full();
    info.remove_subtask(info.task_key(), "gen_texture");
    for (auto &buffer_obj : m_tooltip_buffers) {
      buffer_obj.in_a.unmap();
      buffer_obj.in_b.unmap();
      buffer_obj.out.unmap();
    }
  }

  void ErrorViewerTask::eval(detail::SchedulerHandle &info) {
    met_trace_full();

    if (ImGui::Begin("Error viewer")) {
      // Get shared resources
      auto &e_appl_data = info.get_resource<ApplicationData>(global_key, "app_data");
      auto &e_txtr_data = e_appl_data.loaded_texture_f32;
      auto &e_proj_data = e_appl_data.project_data;
      auto &e_mappings  = e_proj_data.color_systems;

      // Get subtask names
      auto texture_subtask_name  = fmt::format("{}.gen_texture", info.task_key());
      auto resample_subtask_name = fmt::format("{}.gen_resample", texture_subtask_name);

      // Local state
      bool handle_toolip = false;

      // 1. Handle error computation
      eval_error(info);

      // 2. Handle texture resampling subtask
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      float texture_aspect = static_cast<float>(e_txtr_data.size()[1]) / static_cast<float>(e_txtr_data.size()[0]);
      auto texture_size    = (viewport_size * texture_aspect).max(1.f).cast<uint>().eval();

      // Check if the resample subtask needs readjusting for a resized output texture
      if (!texture_size.isApprox(m_texture_size)) {
        m_texture_size = texture_size;

        // Remove previous resample subtask and insert a new one
        ResampleSubtask task = {{ .input_key    = { texture_subtask_name, "colr_texture"        },
                                  .output_key   = { "blablabla", "colr_texture"       },
                                  .texture_info = { .size = m_texture_size                      },
                                  .sampler_info = { .min_filter = gl::SamplerMinFilter::eLinear,
                                                    .mag_filter = gl::SamplerMagFilter::eLinear }}};
        info.remove_subtask(texture_subtask_name, "gen_resample");
        info.insert_subtask(texture_subtask_name, "gen_resample", std::move(task));
      }

      // 3. Display ImGui components to show error and select mapping
      if (info.has_resource(resample_subtask_name, "colr_texture")) {
        auto &e_texture = info.get_resource<gl::Texture2d4f>(resample_subtask_name, "colr_texture");

        ImGui::Image(ImGui::to_ptr(e_texture.object()), m_texture_size.cast<float>().eval());

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