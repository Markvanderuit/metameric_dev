#include <metameric/core/detail/trace.hpp>
#include <metameric/components/views/task_create_project.hpp>
#include <small_gl/window.hpp>
#include <implot.h>

namespace met {
  constexpr float img_rel_width = 128.f;
  constexpr float img_sec_height = img_rel_width + 80.f;
  constexpr float plot_height   = 96.f;

  constexpr auto leaf_flags        = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_SpanFullWidth;
  constexpr auto plot_flags        = ImPlotFlags_NoFrame | ImPlotFlags_NoMenus;
  constexpr auto plot_y_axis_flags = ImPlotAxisFlags_NoDecorations;
  constexpr auto plot_x_axis_flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoGridLines;
  
  CreateProjectTask::CreateProjectTask(const std::string &view_title)
  : m_view_title(view_title) { }

  void CreateProjectTask::init(SchedulerHandle &info) {
    met_trace_full();
    m_proj_data = { };
  }

  void CreateProjectTask::dstr(SchedulerHandle &info) {
    met_trace_full();
    m_proj_data = { };
  }

  void CreateProjectTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_window = info.resource(global_key, "window").read_only<gl::Window>();
    
    if (ImGui::BeginPopupModal(m_view_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      // Primary image and data sections are nested within headers
      if (ImGui::CollapsingHeader("Image data", ImGuiTreeNodeFlags_DefaultOpen))
        eval_images_section(info);
      if (ImGui::CollapsingHeader("Spectral data", ImGuiTreeNodeFlags_DefaultOpen))
        eval_data_section(info);
      
      // Define create/cancel buttons to handle results 
      ImGui::Separator();
      if (m_proj_data.images.empty()) ImGui::BeginDisabled();
      if (ImGui::Button("Create") && create_project_safe(info)) { 
        ImGui::CloseAnyPopupIfOpen();
        info.task(info.task_key()).dstr();
      }
      if (m_proj_data.images.empty()) ImGui::EndDisabled();
      ImGui::SameLine();      
      if (ImGui::Button("Cancel")) { 
        ImGui::CloseCurrentPopup();
        info.task(info.task_key()).dstr();
      }
      
      // Define convex hull vertex slider
      uint min_chull_v = 4, max_chull_v = mvc_weights;
      ImGui::SameLine(0.f, -48.f * e_window.content_scale());
      ImGui::SetNextItemWidth(-48.f * e_window.content_scale());
      ImGui::SliderScalar("Vertices", ImGuiDataType_U32, 
        &m_proj_data.n_vertices, &min_chull_v, &max_chull_v);

      // Insert modals
      eval_progress_modal(info);

      ImGui::EndPopup();
    }
  }
  
  void CreateProjectTask::eval_images_section(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_window = info.resource(global_key, "window").read_only<gl::Window>();

    const float child_height = img_sec_height * e_window.content_scale();
    if (ImGui::BeginChild("##images_wrapper", { 2 * child_height, child_height }, false, ImGuiWindowFlags_HorizontalScrollbar)) {
      ImGui::AlignTextToFramePadding();

      // Track id of image to erase after full draw
      int erased_image = -1;

      for (uint i = 0; i < m_proj_data.images.size(); ++i) {
        const auto &[name, img] = m_imag_data[i];
        auto &img_data = m_proj_data.images[i];

        const float img_width  = img_rel_width * e_window.content_scale();
        const float img_height = img_width * (img.size().x() / img.size().y());

        // Begin wrapper group around image and related content
        if (ImGui::BeginChild(fmt::format("image_{}", i).c_str(), { img_width, 0 }, false)) {
          // Image title
          ImGui::Text(name.c_str());

          // Image delete button at end of line
          ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.f * e_window.content_scale());
          if (ImGui::SmallButton("X")) {
            erased_image = i;
          }

          // Plot image in full pre-calculated width/height
          ImGui::Image(ImGui::to_ptr(img.object()), { img_width, img_height });
          
          // Selector for color matching functions
          ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
          if (ImGui::BeginCombo("##cmfs_selector", m_proj_data.cmfs[img_data.cmfs].first.c_str())) {
            for (uint j = 0; j < m_proj_data.cmfs.size(); ++j) {
              if (ImGui::Selectable(m_proj_data.cmfs[j].first.c_str(), j == img_data.cmfs)) {
                img_data.cmfs = j;
              }
            }
            ImGui::EndCombo();
          }
          ImGui::PopItemWidth();
          
          // Selector for illuminant SPD
          ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
          if (ImGui::BeginCombo("##illuminant_selector", m_proj_data.illuminants[img_data.illuminant].first.c_str())) {
            for (uint j = 0; j < m_proj_data.illuminants.size(); ++j) {
              if (ImGui::Selectable(m_proj_data.illuminants[j].first.c_str(), j == img_data.illuminant)) {
                img_data.illuminant = j;
              }
            }
            ImGui::EndCombo();
          }
          ImGui::PopItemWidth();

        } // End wrapper group around image
        ImGui::EndChild();

        if (i < m_imag_data.size() - 1)
          ImGui::SameLine();
      }

      if (erased_image != -1) {
        m_imag_data.erase(m_imag_data.begin() + erased_image);
        m_proj_data.images.erase(m_proj_data.images.begin() + erased_image);
      }
    }
    ImGui::EndChild();

    // Image selector button
    ImGui::Separator();
    if (std::vector<fs::path> paths; ImGui::Button("Add images...") && detail::load_dialog_mult(paths)) {
      for (const fs::path &path : paths) {
        // Load image with gamma correction applied
        auto host_image = io::load_texture2d<Colr>(path);
        if (path.extension().string() == ".exr") io::to_srgb(host_image);
        
        // Copy this image to gpu for direct display
        auto device_image = gl::Texture2d3f {{ .size = host_image.size(),
                                               .data = cast_span<const float>(host_image.data()) }};

        // Strip gamma correction after copy for the rest of program pipeline
        io::to_lrgb(host_image);

        // Push on list of input 
        m_proj_data.images.push_back({ .image = std::move(host_image),
                                        .cmfs = 0,
                                        .illuminant = 0 });
        m_imag_data.push_back({ path.filename().replace_extension().string(), std::move(device_image) });
      }
    }
  }

  void CreateProjectTask::eval_data_section(SchedulerHandle &info) {
    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);

    // Get shared resources
    const auto &e_window = info.resource(global_key, "window").read_only<gl::Window>();
    
    // Get wavelength values for x-axis in plots
    Spec x_values;
    for (uint i = 0; i < x_values.size(); ++i)
      x_values[i] = wavelength_at_index(i);

    if (ImGui::TreeNodeEx("Illuminants", ImGuiTreeNodeFlags_DefaultOpen)) {
      for (uint i = 0; i < m_proj_data.illuminants.size(); ++i) {
        ImGui::PushID(fmt::format("illuminant_data_{}", i).c_str());

        // Get illuminant data
        auto &[key, illuminant] = m_proj_data.illuminants[i];
                              
        // Draw bulleted leaf node, wrapped in group for hover detection;
        // inside sits a button to delete the relevant spectrum
        ImGui::BeginGroup();
        ImGui::Bullet();
        ImGui::Text(key.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.f * e_window.content_scale());
        if (ImGui::SmallButton("X")) {
          m_proj_data.illuminants.erase(m_proj_data.illuminants.begin() + i);
          for (auto &image : m_proj_data.images)
            if (image.illuminant > 0 && image.illuminant >= i) image.illuminant--;
        }
        ImGui::EndGroup();
        
        // Plot spectral data on tooltip hover over group
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          if (ImPlot::BeginPlot(key.c_str(), { 0., plot_height * e_window.content_scale() }, plot_flags)) {
            ImPlot::SetupAxes("Wavelength", "Value", plot_x_axis_flags, plot_y_axis_flags);
            ImPlot::PlotLine("##plot_line", x_values.data(), illuminant.data(), wavelength_samples);
            ImPlot::PlotShaded("##plot_line", x_values.data(), illuminant.data(), wavelength_samples);
            ImPlot::EndPlot();
          }
          ImGui::EndTooltip();
        }

        ImGui::PopID();
      } // for (uint i)
      ImGui::TreePop();
    }
    
    if (ImGui::TreeNodeEx("Color matching functions", ImGuiTreeNodeFlags_DefaultOpen)) {
      for (uint i = 0; i < m_proj_data.cmfs.size(); ++i) {
        ImGui::PushID(fmt::format("cmfs_data_{}", i).c_str());
        
        // Get cmfs column data separately, as it is stored row-major
        auto &[key, cmfs] = m_proj_data.cmfs[i];
        Spec cmfs_x = cmfs.col(0), cmfs_y = cmfs.col(1), cmfs_z = cmfs.col(2);
        
        // Draw bulleted leaf node, wrapped in group for hover detection;
        // inside sits a button to delete the relevant spectrum
        ImGui::BeginGroup();
        ImGui::Bullet();
        ImGui::Text(key.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.f * e_window.content_scale());
        if (ImGui::SmallButton("X")) {
          m_proj_data.cmfs.erase(m_proj_data.cmfs.begin() + i);
          for (auto &image : m_proj_data.images)
            if (image.cmfs > 0 && image.cmfs >= i) image.cmfs--;
        }
        ImGui::EndGroup();

        // Plot spectral data on tooltip hover over group
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          if (ImPlot::BeginPlot(key.c_str(), { 0., plot_height * e_window.content_scale() }, plot_flags)) {
            ImPlot::SetupAxes("Wavelength", "Value", plot_x_axis_flags, plot_y_axis_flags);
            ImPlot::PlotLine("x", x_values.data(), cmfs_x.data(), wavelength_samples);
            ImPlot::PlotLine("y", x_values.data(), cmfs_y.data(), wavelength_samples);
            ImPlot::PlotLine("z", x_values.data(), cmfs_z.data(), wavelength_samples);
            ImPlot::PlotShaded("x", x_values.data(), cmfs_x.data(), wavelength_samples);
            ImPlot::PlotShaded("y", x_values.data(), cmfs_y.data(), wavelength_samples);
            ImPlot::PlotShaded("z", x_values.data(), cmfs_z.data(), wavelength_samples);
            ImPlot::EndPlot();
          }
          ImGui::EndTooltip();
        }

        ImGui::PopID();
      } // for (uint i)
      ImGui::TreePop();
    }

    ImGui::Separator();
      
    // Illuminant/cmfs load buttons
    if (std::vector<fs::path> paths; ImGui::Button("Add illuminants") && detail::load_dialog_mult(paths)) {
      for (const fs::path &path : paths) {
          Spec spec = io::load_spec(path);
          auto name = path.filename().replace_extension().string();
          m_proj_data.illuminants.push_back({ name, spec });
      }
    }
    ImGui::SameLine();
    if (std::vector<fs::path> paths; ImGui::Button("Add cmfs") && detail::load_dialog_mult(paths)) {
      for (const fs::path &path : paths) {
        CMFS cmfs = io::load_cmfs(path);
        auto name = path.filename().replace_extension().string();
        m_proj_data.cmfs.push_back({ name, cmfs });
      }
    }

    ImPlot::PopStyleVar();
  }
  
  void CreateProjectTask::eval_progress_modal(SchedulerHandle &info) {
    if (ImGui::BeginPopupModal("Warning: unsaved progress")) {
      ImGui::Text("If you continue, you may lose unsaved progress.");
      ImGui::Separator();
      if (ImGui::Button("Continue")) {
        create_project(info);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
    }
  }

  bool CreateProjectTask::create_project_safe(SchedulerHandle &info) {
    const auto &e_app_data = info.resource(global_key, "app_data").read_only<ApplicationData>();
    if (e_app_data.project_save == SaveFlag::eUnsaved || e_app_data.project_save == SaveFlag::eNew) {
      ImGui::OpenPopup("Warning: unsaved progress", 0);
      return false;
    /* } else if (!fs::exists(m_input_path)) {
      ImGui::OpenPopup("Warning: file not found", 0);
      return false; */
    } else {
      return create_project(info);
    }
  }

  bool CreateProjectTask::create_project(SchedulerHandle &info) {
    // Create a new project
    info.resource(global_key, "app_data").writeable<ApplicationData>().create(std::move(m_proj_data));

    // Signal schedule re-creation and submit new task schedule
    submit_schedule_main(info);

    return true;
  }
} // namespace met