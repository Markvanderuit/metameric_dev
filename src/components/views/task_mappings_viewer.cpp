#include <metameric/core/data.hpp>
#include <metameric/core/io.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/core/detail/scheduler_base.hpp>
#include <metameric/components/views/task_mappings_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <implot.h>
#include <small_gl/texture.hpp>

namespace met {
  void MappingsViewerTask::eval_tooltip_copy(SchedulerHandle &info, uint texture_i) {
    met_trace_full();

    // Get external resources
    const auto &e_bary_buffer = info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>();
    const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_colr_data   = e_appl_data.loaded_texture;

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos = (static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                           -  static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                           /  static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    m_tooltip_pixel = (mouse_pos * e_colr_data.size().cast<float>()).cast<int>();
    const size_t pos = e_colr_data.size().x() * m_tooltip_pixel.y() + m_tooltip_pixel.x();

    // Perform copy of relevant barycentric data to current available buffer
    size_t bary_size = e_proj_data.meshing_type == ProjectMeshingType::eDelaunay ? sizeof(eig::Array4f) : sizeof(Bary); 
    e_bary_buffer.copy_to(m_tooltip_buffers[m_tooltip_cycle_i], bary_size, bary_size * pos);

    // Submit a fence for current available buffer as it affects mapped memory
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_tooltip_fences[m_tooltip_cycle_i] = gl::sync::Fence(gl::sync::time_s(1));
  }

  void MappingsViewerTask::eval_tooltip(SchedulerHandle &info, uint texture_i) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    const auto &e_vert_spec = info("gen_spectral_data", "vert_spec").read_only<std::vector<Spec>>();

    // Spawn tooltip
    ImGui::BeginTooltip();
    ImGui::Text("Inspecting pixel (%i, %i)", m_tooltip_pixel.x(), m_tooltip_pixel.y());
    ImGui::Separator();

    // Acquire output barycentric data, which should by now be copied into the next buffer
    // Check fence for this buffer, however, in case this is not the case
    m_tooltip_cycle_i = (m_tooltip_cycle_i + 1) % m_tooltip_buffers.size();
    if (auto &fence = m_tooltip_fences[m_tooltip_cycle_i]; fence.is_init())
      fence.cpu_wait();

    // Unpack barycentric weights and get corresponding tetrahedron from delaunay
    if (auto rsrc = info("gen_convex_weights", "delaunay"); rsrc.is_init()) {
      const auto &e_delaunay = rsrc.read_only<AlignedDelaunayData>();
      const auto &bary_data = std::get<std::span<eig::Array4f>>(m_tooltip_maps[m_tooltip_cycle_i])[0];

      eig::Array4f bary   = (eig::Array4f() << bary_data.head<3>(), 1.f - bary_data.head<3>().sum()).finished(); 
      uint         bary_i = std::min(*reinterpret_cast<const uint *>(&bary_data.w()), static_cast<uint>(e_delaunay.elems.size()) - 1);
      eig::Array4u elem   = e_delaunay.elems[bary_i].min(e_delaunay.verts.size() - 1);

      // Compute output reflectance and color as convex combinations
      Spec reflectance = 0;
      for (uint i = 0; i < 4; ++i)
        reflectance += bary[i] * e_vert_spec[elem[i]];
      ColrSystem mapp  = e_proj_data.csys(texture_i);
      Colr color = mapp.apply_color_indirect(reflectance);
      Spec power = mapp.illuminant * reflectance;

      ImGui::PlotLines("Reflectance", reflectance.data(), wavelength_samples, 0,
        nullptr, 0.f, 1.f, { 0.f, 64.f });
      ImGui::PlotLines("Power", power.data(), wavelength_samples, 0,
        nullptr, 0.f, power.maxCoeff(), { 0.f, 64.f });

      ImGui::ColorEdit3("Color (sRGB)", lrgb_to_srgb(color).data(), ImGuiColorEditFlags_Float);

      ImGui::Separator();

      ImGui::InputScalarN("Weights", ImGuiDataType_Float, bary.data(), bary.size());
      ImGui::InputScalarN("Indices", ImGuiDataType_U32, elem.data(), elem.size());

      ImGui::Separator();
      
      ImGui::Value("Minimum", reflectance.minCoeff(), "%.6f");
      ImGui::Value("Maximum", reflectance.maxCoeff(), "%.6f");
      ImGui::Value("Bounded", reflectance.minCoeff() >= 0.f && reflectance.maxCoeff() <= 1.f);
    } else {
      const auto &bary_data = std::get<std::span<Bary>>(m_tooltip_maps[m_tooltip_cycle_i])[0];
    
      // Compute output reflectance and color as convex combinations
      Bary bary = bary_data;
      Spec reflectance = 0;
      for (uint i = 0; i < e_vert_spec.size(); ++i)
        reflectance += bary[i] * e_vert_spec[i];
      ColrSystem mapp  = e_proj_data.csys(texture_i);
      Colr color = mapp.apply_color_indirect(reflectance);
      Spec power = mapp.illuminant * reflectance;

      ImGui::PlotLines("Reflectance", reflectance.data(), wavelength_samples, 0,
        nullptr, 0.f, 1.f, { 0.f, 64.f });
      ImGui::PlotLines("Power", power.data(), wavelength_samples, 0,
        nullptr, 0.f, power.maxCoeff(), { 0.f, 64.f });

      ImGui::ColorEdit3("Color (sRGB)", lrgb_to_srgb(color).data(), ImGuiColorEditFlags_Float);

      ImGui::Separator();

      ImGui::PlotLines("Weights", bary.data(), bary.size(), 0, 
        nullptr, 0.f, 1.f, { 0.f, 64.f });

      ImGui::Separator();
      
      ImGui::Value("Minimum", reflectance.minCoeff(), "%.6f");
      ImGui::Value("Maximum", reflectance.maxCoeff(), "%.6f");
      ImGui::Value("Bounded", reflectance.minCoeff() >= 0.f && reflectance.maxCoeff() <= 1.f);
    }

    ImGui::EndTooltip();
  }

  void MappingsViewerTask::eval_popout(SchedulerHandle &info, uint texture_i) {
    // ...
  }

  void MappingsViewerTask::eval_save(SchedulerHandle &info, uint texture_i) {
    if (fs::path path; detail::save_dialog(path, "bmp,png,jpg,exr")) {
      // Get external resources
      auto color_task_key = fmt::format("gen_color_mapping_{}", texture_i);
      const auto &e_colr_buffer = info(color_task_key, "colr_buffer").read_only<gl::Buffer>();
      const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();

      // Obtain cpu-side texture
      Texture2d3f_al texture_al = {{ .size = e_appl_data.loaded_texture.size() }};
      e_colr_buffer.get(cast_span<std::byte>(texture_al.data()));

      // Remove padding bytes, then save to disk
      io::save_texture2d(path, io::as_unaligned(texture_al), true);
    }
  }

  void MappingsViewerTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    
    m_resample_size   = 1;
    m_tooltip_cycle_i = 0;

    // Initialize a set of rolling buffers of size Spec, and map these for reading
    constexpr auto create_flags = gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapRead;
    constexpr auto map_flags    = gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapRead;
    for (uint i = 0; i < m_tooltip_buffers.size(); ++i) {
      auto &buffer = m_tooltip_buffers[i];
      auto &map    = m_tooltip_maps[i];
      if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
        buffer = {{ .size = sizeof(eig::Array4f), .flags = create_flags }};
        map = cast_span<eig::Array4f>(buffer.map(map_flags));
      } else {
        buffer = {{ .size = sizeof(Bary), .flags = create_flags }};
        map = cast_span<Bary>(buffer.map(map_flags));
      }
    }

    // Initialize texture generation subtasks
    auto e_texture_size = e_appl_data.loaded_texture.size();
    uint e_mappings_n = e_proj_data.color_systems.size();
    m_texture_subtasks.init(info, e_mappings_n, [](uint i) { return fmt::format("gen_texture_{}", i); },
    [=](auto &, uint i) { return TextureSubTask {{ 
      .input_key = { fmt::format("gen_color_mappings.gen_mapping_{}", i), "colr_buffer" }, 
      .output_key    = "texture",
      .texture_info  = { .size = e_texture_size } 
    }}; });
      
    // Initialize texture resampling subtasks
    std::string parent_task = fmt::format("{}.gen_texture", info.task().key());
    m_resample_subtasks.init(info, e_mappings_n, [](uint i) { return fmt::format("gen_resample_{}", i); },
    [=](auto &, uint i) { return ResampleSubtask {{ 
      .input_key = { fmt::format("{}_{}", parent_task, i), "texture" }, 
      .output_key    = "texture",
      .texture_info  = { .size = 1u }, 
      .sampler_info  = { .min_filter = gl::SamplerMinFilter::eLinear, .mag_filter = gl::SamplerMagFilter::eLinear },
      .lrgb_to_srgb  = true 
    }}; });
  }

  void MappingsViewerTask::eval(SchedulerHandle &info) {
    met_trace_full();
    
    if (ImGui::Begin("Mappings viewer")) {
      // Get shared resources
      const auto &e_pipe_state = info("state", "pipeline_state").read_only<ProjectState>();
      const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_proj_data  = e_appl_data.project_data;
      uint e_mappings_n = e_proj_data.color_systems.size();
      
      // Set up drawing a nr. of textures in a column-based layout; determine texture res.
      uint n_cols = 2;
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      eig::Array2f texture_size = viewport_size
                                 * e_appl_data.loaded_texture.size().cast<float>().y()
                                 / e_appl_data.loaded_texture.size().cast<float>().x()
                                 * 0.95f / static_cast<float>(n_cols);

      // Adjust nr. of subtasks for texture generation
      m_texture_subtasks.eval(info, e_mappings_n);
      m_resample_subtasks.eval(info, e_mappings_n);

      // Notify subtasks of potential input changes and handle texture size change
      m_resample_size = texture_size.max(1.f).cast<uint>();
      for (uint i = 0; i < e_mappings_n; ++i) {
        // Subtask names
        std::string gen_name = fmt::format("gen_texture_{}", i);
        std::string res_name = fmt::format("gen_resample_{}", i);
        
        guard_continue(info.subtask(gen_name).is_init() && info.subtask(res_name).is_init());
        
        // Notify resample subtask of potential resize
        auto task = info.subtask(res_name);
        auto mask = task.mask(info);
        task.realize<ResampleSubtask>().set_texture_info(mask, { .size = m_resample_size });
      }

      // Reset state for tooltip
      m_tooltip_mapping_i = -1;
      
      // Iterate n_cols, n_rows, and n_mappings
      for (uint i = 0, i_col = 0; i < e_mappings_n; ++i) {
        ImGui::PushID(fmt::format("mapping_viewer_texture_{}", i).c_str());

        // Subtask names
        std::string gen_name = fmt::format("{}.gen_texture_{}", info.task().key(), i);
        std::string res_name = fmt::format("{}.gen_resample_{}", info.task().key(), i);
        
        // Get externally shared resources; note, resources may not be created yet as tasks are
        // added into the schedule at the end of a loop, not during
        guard_continue(info.task(gen_name).is_init() || info.task(res_name).is_init());
        const auto &e_texture = info(res_name, "texture").read_only<gl::Texture2d4f>();

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