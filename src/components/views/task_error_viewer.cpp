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

  void ErrorViewerTask::eval_tooltip_copy(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_txtr_data    = info.global("appl_data").read_only<ApplicationData>().loaded_texture;
    const auto &e_color_input  = info.resource("gen_convex_weights", "colr_buffer").read_only<gl::Buffer>();
    const auto &e_color_output = info.resource("gen_color_mappings.gen_mapping_0", "colr_buffer").read_only<gl::Buffer>();

    // Get modified resources
    auto &i_color_error = info.resource("colr_buffer").writeable<gl::Buffer>();

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

  void ErrorViewerTask::eval_tooltip(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_window = info.global("window").read_only<gl::Window>();

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

  void ErrorViewerTask::eval_error(SchedulerHandle &info) {
    // Continue only on relevant state changes
    guard(info.resource("state", "proj_state").read_only<ProjectState>().is_any_stale);

    // Get external resources
    const auto &e_color_input  = info("gen_convex_weights", "colr_buffer").read_only<gl::Buffer>();
    const auto &e_color_output = info("gen_color_mappings.gen_mapping_0", "colr_buffer").read_only<gl::Buffer>();

    // Get modified resources
    auto &i_color_error = info("colr_buffer").writeable<gl::Buffer>();

    // Bind resources
    m_error_program.bind("b_unif", m_error_uniform);
    m_error_program.bind("b_in_a", e_color_input);
    m_error_program.bind("b_in_b", e_color_output);
    m_error_program.bind("b_err",  info("colr_buffer").writeable<gl::Buffer>());

    // Dispatch shader to generate spectral dataw
    gl::sync::memory_barrier(gl::BarrierFlags::eShaderStorageBuffer);
    gl::dispatch_compute(m_error_dispatch);
  }

  void ErrorViewerTask::init(SchedulerHandle &info) {
    met_trace_full();

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
    const auto &e_txtr_data = info.global("appl_data").read_only<ApplicationData>().loaded_texture;

    // Initialize error computation components
    const uint dispatch_n    = e_txtr_data.size().prod();
    const uint dispatch_ndiv = ceil_div(dispatch_n, 256u);
    m_error_program = {{ .type = gl::ShaderType::eCompute,
                         .spirv_path = "resources/shaders/views/draw_error.comp.spv",
                         .cross_path = "resources/shaders/views/draw_error.comp.json" }};
    m_error_dispatch = { .groups_x = dispatch_ndiv, 
                         .bindable_program = &m_error_program };
    m_error_uniform = {{ .data = obj_span<const std::byte>(dispatch_n) }};
    
    // Insert buffer object to hold error data
    info.resource("colr_buffer").init<gl::Buffer>({ .size = dispatch_n * sizeof(AlColr) });

    // Create subtask to handle buffer->texture copy
    TextureSubtask texture_subtask = {{ .input_key    = { info.task().key(), "colr_buffer" }, .output_key   = "colr_texture",
                                        .texture_info = { .size = e_txtr_data.size() }}};
                                
    // Create subtask to handle texture->texture resampling and gamma correction
    ResampleSubtask resample_subtask = {{ .input_key    = { fmt::format("{}.gen_texture", info.task().key()), "colr_texture" }, .output_key   = "colr_texture",
                                          .texture_info = { .size = 1u }, .sampler_info = { .min_filter = gl::SamplerMinFilter::eLinear, .mag_filter = gl::SamplerMagFilter::eLinear }}};
    
    // Submit subtasks to scheduler
    info.subtask("gen_texture").set(std::move(texture_subtask));
    info.subtask("gen_resample").set(std::move(resample_subtask));
  }

  void ErrorViewerTask::eval(SchedulerHandle &info) {
    met_trace_full();

    if (ImGui::Begin("Error viewer")) {
      // Get external resources
      const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_txtr_data = e_appl_data.loaded_texture;
      const auto &e_proj_data = e_appl_data.project_data;
      const auto &e_mappings  = e_proj_data.color_systems;

      // Get subtask names
      auto texture_subtask_name  = fmt::format("{}.gen_texture", info.task().key());
      auto resample_subtask_name = fmt::format("{}.gen_resample", info.task().key());

      // Local state
      bool handle_toolip = false;

      // 1. Handle error computation
      eval_error(info);

      // 2. Handle texture resampling subtask
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      float texture_aspect = static_cast<float>(e_txtr_data.size()[1]) / static_cast<float>(e_txtr_data.size()[0]);
      auto texture_size    = (viewport_size * texture_aspect).max(1.f).cast<uint>().eval();

      // Ensure the resample subtask can readjust for a resized output texture
      {
        auto task = info.subtask("gen_resample");
        auto mask = task.mask(info);
        task.realize<ResampleSubtask>().set_texture_info(mask, { .size = texture_size });
      }

      // 3. Display ImGui components to show error and select mapping
      if (auto rsrc = info.resource(resample_subtask_name, "colr_texture"); rsrc.is_init()) {
        ImGui::Image(ImGui::to_ptr(rsrc.read_only<gl::Texture2d4f>().object()), texture_size.cast<float>().eval());

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