#include <metameric/core/data.hpp>
#include <metameric/core/mesh.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_bary_viewer.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

namespace met {
  constexpr float tooltip_width = 256.f;

  void BaryViewerTask::eval_tooltip_copy(SchedulerHandle &info) {
    met_trace_full();

    auto subtask_name  = fmt::format("{}.gen_mapping", info.task().key());

    // Get external resources
    const auto &e_bary_buffer = info("gen_convex_weights", "bary_buffer").read_only<gl::Buffer>();
    const auto &e_appl_data   = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data   = e_appl_data.project_data;
    const auto &e_colr_data   = e_appl_data.loaded_texture;

    // Compute sample position in texture dependent on mouse position in image
    eig::Array2f mouse_pos =(static_cast<eig::Array2f>(ImGui::GetMousePos()) 
                           - static_cast<eig::Array2f>(ImGui::GetItemRectMin()))
                           / static_cast<eig::Array2f>(ImGui::GetItemRectSize());
    m_tooltip_pixel = (mouse_pos * e_colr_data.size().cast<float>()).cast<int>();
    const size_t pos = e_colr_data.size().x() * m_tooltip_pixel.y() + m_tooltip_pixel.x();

    // Perform copy of relevant barycentric data to current available buffer
    size_t bary_size = e_proj_data.meshing_type == ProjectMeshingType::eConvexHull ? sizeof(Bary) : sizeof(eig::Array4f); 
    e_bary_buffer.copy_to(m_tooltip_buffers[m_tooltip_cycle_i], bary_size, bary_size * pos);

    // Submit a fence for current available buffer as it affects mapped memory
    gl::sync::memory_barrier(gl::BarrierFlags::eClientMappedBuffer);
    m_tooltip_fences[m_tooltip_cycle_i] = gl::sync::Fence(gl::sync::time_s(1));
  } 

  void BaryViewerTask::eval_tooltip(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    const auto &e_colr_data = e_appl_data.loaded_texture;
    const auto &e_vert_spec = info("gen_spectral_data", "spectra").read_only<std::vector<Spec>>();
    const auto &e_window    = info.global("window").read_only<gl::Window>();

    // Spawn tooltip
    ImGui::BeginTooltip();
    ImGui::SetNextItemWidth(tooltip_width * e_window.content_scale());
    ImGui::Text("Inspecting pixel (%i, %i)", m_tooltip_pixel.x(), m_tooltip_pixel.y());
    ImGui::Separator();

    // Acquire output barycentric data, which should by now be copied into the next buffer
    // Check fence for this buffer, however, in case this is not the case
    m_tooltip_cycle_i = (m_tooltip_cycle_i + 1) % m_tooltip_buffers.size();
    if (auto &fence = m_tooltip_fences[m_tooltip_cycle_i]; fence.is_init())
      fence.cpu_wait();

    // Unpack correct form of barycentric weights and compute reflectance
    Spec reflectance = 0;
    if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
      const auto &bary_data = std::get<std::span<Bary>>(m_tooltip_maps[m_tooltip_cycle_i])[0];
    
      // Compute output reflectance as convex combinations
      Bary bary = bary_data;
      for (uint i = 0; i < e_vert_spec.size(); ++i)
        reflectance += bary[i] * e_vert_spec[i];
    } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
      const auto e_delaunay = info("gen_convex_weights", "delaunay").read_only<AlignedDelaunayData>();
      const auto &bary_data = std::get<std::span<eig::Array4f>>(m_tooltip_maps[m_tooltip_cycle_i])[0];

      eig::Array4f bary   = (eig::Array4f() << bary_data.head<3>(), 1.f - bary_data.head<3>().sum()).finished(); 
      uint         bary_i = std::min(*reinterpret_cast<const uint *>(&bary_data.w()), static_cast<uint>(e_delaunay.elems.size()) - 1);
      eig::Array4u elem   = e_delaunay.elems[bary_i].min(e_delaunay.verts.size() - 1);

      // Compute output reflectance as convex combination
      for (uint i = 0; i < 4; ++i)
        reflectance += bary[i] * e_vert_spec[elem[i]];
    }

    // Get reflectance-related data
    ColrSystem mapp  = e_proj_data.csys(0);
    Spec power = mapp.illuminant * reflectance;

    // Get color error
    Colr color_in   = e_colr_data[m_tooltip_pixel];
    Colr color_out  = mapp.apply_color_indirect(reflectance);
    Colr color_err  = (color_out - color_in).abs(); 

    // Plot spectra
    ImGui::PlotLines("Reflectance", reflectance.data(), wavelength_samples, 0, nullptr, 0.f, 1.f, { 0.f, 64.f });
    ImGui::PlotLines("Power", power.data(), wavelength_samples, 0, nullptr, 0.f, power.maxCoeff(), { 0.f, 64.f });

    ImGui::Separator();

    // Plot output color information
    ImGui::ColorEdit3("Input (sRGB)", lrgb_to_srgb(color_in).data(), ImGuiColorEditFlags_Float);
    ImGui::ColorEdit3("Rtrip (sRGB)", lrgb_to_srgb(color_out).data(), ImGuiColorEditFlags_Float);
    ImGui::ColorEdit3("Error (lRGB)", color_err.data(), ImGuiColorEditFlags_Float);
    
    ImGui::EndTooltip();
  }

  void BaryViewerTask::init(SchedulerHandle &info) {
    met_trace_full();

    // Get external resources
    const auto &e_appl_data = info.global("appl_data").read_only<ApplicationData>();
    const auto &e_proj_data = e_appl_data.project_data;
    
    // Initialize a set of rolling buffers, and map these for reading
    constexpr auto create_flags = gl::BufferCreateFlags::eMapPersistent | gl::BufferCreateFlags::eMapRead;
    constexpr auto map_flags    = gl::BufferAccessFlags::eMapPersistent | gl::BufferAccessFlags::eMapRead;
    for (uint i = 0; i < m_tooltip_buffers.size(); ++i) {
      auto &buffer = m_tooltip_buffers[i];
      auto &map    = m_tooltip_maps[i];
      if (e_proj_data.meshing_type == ProjectMeshingType::eConvexHull) {
        buffer = {{ .size = sizeof(Bary), .flags = create_flags }};
        map = cast_span<Bary>(buffer.map(map_flags));
      } else if (e_proj_data.meshing_type == ProjectMeshingType::eDelaunay) {
        buffer = {{ .size = sizeof(eig::Array4f), .flags = create_flags }};
        map = cast_span<eig::Array4f>(buffer.map(map_flags));
      }
    }

    // Initialize error texture generation subtask
    info.subtask("gen_bary").init<GenBaryMappingTask>(0);

    m_tooltip_cycle_i = 0;
  }

  void BaryViewerTask::eval(SchedulerHandle &info) {
    met_trace_full();

    if (ImGui::Begin("Weight viewer")) {
      // Get external resources
      const auto &e_appl_data  = info.global("appl_data").read_only<ApplicationData>();
      const auto &e_txtr_data  = e_appl_data.loaded_texture;
      const auto &e_proj_data  = e_appl_data.project_data;
      const auto &e_mappings   = e_proj_data.color_systems;
      const auto &e_cstr_selct = info("viewport.overlay", "constr_selection").read_only<int>();

      // Local state
      bool handle_toolip = false;

      // 1. Handle texture size determination
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      float texture_aspect       = static_cast<float>(e_txtr_data.size()[1]) / static_cast<float>(e_txtr_data.size()[0]);
      eig::Array2f texture_size  = (viewport_size * texture_aspect).max(1.f).eval();

      auto subtask_name = fmt::format("{}.gen_bary", info.task().key());

      // 2. Handle error texture generation subtask resize
      if (auto handle = info.task(subtask_name); handle.is_init()) {
        auto &subtask = handle.realize<GenBaryMappingTask>();
        auto mask = handle.mask(info);
        subtask.set_texture_info(mask, { .size = texture_size.cast<uint>() });
        subtask.set_cstr_slct(mask, e_cstr_selct);
      }

      // 3. Display ImGui components to show error and select mapping
      if (auto handle = info.resource(subtask_name, "colr_texture"); handle.is_init()) {
        const auto &resource = handle.read_only<gl::Texture2d4f>();
        ImGui::Image(ImGui::to_ptr(resource.object()), texture_size);

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