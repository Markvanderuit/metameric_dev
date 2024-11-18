#include <metameric/core/io.hpp>
#include <metameric/core/image.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/components/views/task_render_export.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/component_edit.hpp>

namespace met {
  constexpr static uint export_spp_per_iter = 4u;

  void RenderExportTask::init(SchedulerHandle &info) {
    met_trace();    
  }
    
  void RenderExportTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_view  = e_scene.components.views[m_view].value;
    auto render_handle  = info("renderer");

    bool is_open = true;
    if (ImGui::Begin("Export view", &is_open)) {
      // Path header
      auto path_str = m_path.string();
      if (ImGui::Button("...")) {
        if (fs::path path; detail::save_dialog(path, { "*.exr" }))
          m_path = path;
      }
      ImGui::SameLine();
      if (ImGui::InputText("Path", &path_str)) {
        m_path = path_str;
      }

      // Renderer selector; path, direct, spectral, rgb etc
      if (ImGui::BeginCombo("Renderer", fmt::format("{}", m_render_type).c_str())) {
        for (uint i = 0; i < 6; ++i) {
          auto type = static_cast<Settings::RendererType>(i);
          auto name = fmt::format("{}", type);
          if (ImGui::Selectable(name.c_str(), m_render_type == type))
            m_render_type = type;
        } // for (uint i)
        ImGui::EndCombo();
      }

      // Samples per pixel
      ImGui::InputScalar("Samples", ImGuiDataType_U32, &m_spp_trgt);

      // Camera selector
      push_resource_selector("View", e_scene.components.views, m_view);
      
      ImGui::Separator();

      // Start/stop/progress bar
      if (!m_in_prog) {
        if (m_path.empty() || m_in_prog)
          ImGui::BeginDisabled();
        if (ImGui::Button("Start render")) {
          m_in_prog  = true;
          m_spp_curr = 0;

          // Kill viewport render task
          info("viewport.viewport_render", "active").set(false);
        }
        if (m_path.empty() || m_in_prog)
          ImGui::EndDisabled();
      } else {
        auto prg = static_cast<float>(m_spp_curr) / static_cast<float>(m_spp_trgt);
        auto str = fmt::format("{} / {} ({})", m_spp_curr, m_spp_trgt, prg);
        fmt::print("render progress: {}\n", str);
        ImGui::ProgressBar(prg, { 0, 0 }, str.c_str());
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button("Cancel")) {
          m_in_prog  = false;
          m_spp_curr = 0;
          
          // Restart viewport render task
          info("viewport.viewport_render", "active").set(true);
        }
      }
    }
    ImGui::End();

    // Window closed, kill this task
    if (!is_open)
      info.task().dstr();

    // Handle render state
    if (m_in_prog) {
      // Begin render
      if (m_spp_curr == 0) {
        // Get shared resources
        m_arcball = info("viewport.viewport_input_camera", "arcball").getr<detail::Arcball>();

        // Specify camera
        {
          eig::Affine3f trf_rot = eig::Affine3f::Identity();
          trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.x(), eig::Vector3f::UnitY());
          trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.y(), eig::Vector3f::UnitX());
          trf_rot *= eig::AngleAxisf(e_view.camera_trf.rotation.z(), eig::Vector3f::UnitZ());

          auto dir = (trf_rot * eig::Vector3f(0, 0, 1)).normalized().eval();
          auto eye = -dir; 
          auto cen = (e_view.camera_trf.position + dir).eval();

          m_arcball.set_zoom(1);
          m_arcball.set_fov_y(e_view.camera_fov_y * std::numbers::pi_v<float> / 180.f);
          m_arcball.set_eye(eye);
          m_arcball.set_center(cen);
          m_arcball.set_aspect(static_cast<float>(e_view.film_size.x()) / static_cast<float>(e_view.film_size.y()));
        }

        // Initialize sensor and renderer primitives
        m_sensor = {};
        m_sensor.film_size = e_view.film_size;
        m_sensor.proj_trf  = m_arcball.proj().matrix();
        m_sensor.view_trf  = m_arcball.view().matrix();
        m_sensor.flush();

        // Initialize renderer
        switch (m_render_type) {
          case Settings::RendererType::ePath:
            render_handle.init<PathRenderPrimitive>({ .spp_per_iter       = export_spp_per_iter,
                                                      .pixel_checkerboard = false,
                                                      .cache_handle       = info.global("cache") });
            break;
          case Settings::RendererType::eDirect:
            render_handle.init<PathRenderPrimitive>({ .spp_per_iter       = export_spp_per_iter,
                                                      .max_depth          = 2u,
                                                      .pixel_checkerboard = false,
                                                      .cache_handle       = info.global("cache") });
            break;
          case Settings::RendererType::eDebug:
            render_handle.init<PathRenderPrimitive>({ .spp_per_iter       = export_spp_per_iter,
                                                      .max_depth          = 2u,
                                                      .pixel_checkerboard = false,
                                                      .enable_debug       = true,
                                                      .cache_handle       = info.global("cache") });
            break;
        }
        render_handle.getw<PathRenderPrimitive>().reset(m_sensor, e_scene);
      }

      // Render step; taken over several frames
      render_handle.getw<PathRenderPrimitive>().render(m_sensor, e_scene);
      m_spp_curr += export_spp_per_iter;

      // End render
      if (m_spp_curr >= m_spp_trgt) {
        m_in_prog = false;

        // Create cpu-side image matching gpu-side film format
        Image image = {{
          .pixel_frmt = Image::PixelFormat::eRGBA,
          .pixel_type = Image::PixelType::eFloat,
          .size       = e_view.film_size
        }};

        // Copy film to cpu-side and save to exr *without* gamma correction
        render_handle.getr<detail::IntegrationRenderPrimitive>()
                     .film().get(cast_span<float>(image.data()));
        image.save_exr(m_path);
        
        // Destroy renderer
        render_handle.dstr();

        // Clear path because I am an idiot sometimes
        m_path.clear();

        // Restart viewport render task
        info("viewport.viewport_render", "active").set(true);
      }
    }
  }
} // namespace met