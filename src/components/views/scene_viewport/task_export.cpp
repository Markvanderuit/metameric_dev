#include <metameric/core/io.hpp>
#include <metameric/core/image.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/components/views/scene_viewport/task_export.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/views/detail/file_dialog.hpp>
#include <metameric/components/views/detail/component_edit.hpp>

namespace met {
  constexpr static uint spp_per_iter = 16u;

  void MeshViewportExportTask::init(SchedulerHandle &info) {
    met_trace();    
  }
    
  void MeshViewportExportTask::eval(SchedulerHandle &info) {
    met_trace_full();

    // Get shared resources
    const auto &e_scene = info.global("scene").getr<Scene>();
    const auto &e_view  = e_scene.components.views[m_view].value;

    bool is_open = true;
    if (ImGui::Begin("Export view", &is_open)) {
      // Path header
      auto path_str = m_path.string();
      if (ImGui::Button("...")) {
        if (fs::path path; detail::save_dialog(path, "exr"))
          m_path = path;
      }
      ImGui::SameLine();
      if (ImGui::InputText("Path", &path_str)) {
        m_path = path_str;
      }

      ImGui::SeparatorText("Render settings");
      
      ImGui::InputScalar("SPP", ImGuiDataType_U32, &m_spp_trgt);
      push_resource_selector("View", e_scene.components.views, m_view);
      
      ImGui::Separator();

      // Start/stop buttons
      if (m_path.empty() || m_in_prog)
        ImGui::BeginDisabled();
      if (ImGui::Button("Start render")) {
        m_in_prog  = true;
        m_spp_curr = 0;

        // Kill viewport render task
        info.relative("viewport_render")("active").set(false);
      }
      if (m_path.empty() || m_in_prog)
        ImGui::EndDisabled();
      ImGui::SameLine();
      if (!m_in_prog)
        ImGui::BeginDisabled();
      if (ImGui::Button("Stop render")) {
        m_in_prog  = false;
        m_spp_curr = 0;
        
        // Restart viewport render task
        info.relative("viewport_render")("active").set(true);
      }
      if (!m_in_prog)
        ImGui::EndDisabled();
      if (m_in_prog) {
        auto prg = static_cast<float>(m_spp_curr) / static_cast<float>(m_spp_trgt);
        auto str = std::format("{} / {} ({})", m_spp_curr, m_spp_trgt, prg);
        ImGui::ProgressBar(prg, { 0, 0 }, str.c_str());
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::Text("Progress");
      }

      if (!m_in_prog && m_spp_curr > 0 && m_render.film().is_init()) {
        // Place texture view using draw target
        ImGui::Image(ImGui::to_ptr(m_render.film().object()), 
          m_render.film().size().cast<float>().eval(), 
          eig::Vector2f(0, 1), eig::Vector2f(1, 0));
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
        m_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();

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
        m_render = {{ .spp_per_iter = spp_per_iter,
                      .max_depth    = 4u,
                      .cache_handle = info.global("cache") }};
        m_render.reset(m_sensor, e_scene);
      }

      // Render step; taken over several frames
      m_render.render(m_sensor, e_scene);
      m_spp_curr += spp_per_iter;

      // End render
      if (m_spp_curr >= m_spp_trgt) {
        m_in_prog = false;

        // Create cpu-side image matching gpu-side film format
        Image image = {{
          .pixel_frmt = Image::PixelFormat::eRGBA,
          .pixel_type = Image::PixelType::eFloat,
          .size       = e_view.film_size
        }};

        // Copy over to cpu-side
        m_render.film().get(cast_span<float>(image.data()));

        // Save to exr; no gamma correction
        image.save_exr(m_path);

        // Clear path because I am an idiot sometimes
        m_path.clear();

        // Restart viewport render task
        info.relative("viewport_render")("active").set(true);
      }
    }
  }
} // namespace met