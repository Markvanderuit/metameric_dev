#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/record.hpp>
#include <metameric/scene/scene.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/render/primitives_query.hpp>
#include <metameric/editor/detail/arcball.hpp>
#include <metameric/editor/detail/imgui.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>
#include <algorithm>
#include <execution>

namespace met {
  class PathMeasureToolTask : public detail::TaskNode {
    PixelSensor m_query_sensor;
    uint        m_query_spp = 0;

  public:
    bool is_active(SchedulerHandle &info) override {
      return info.parent().parent()("is_active").getr<bool>(); // that be the viewport
    }

    void init(SchedulerHandle &info) override {
      met_trace();
      info("path_query").init<PathQueryPrimitive>({ .cache_handle = info.global("cache") });
    } 

    void measure(SchedulerHandle &info) {
      met_trace_full();

      // Get shared resources
      const auto &e_window  = info.global("window").getr<gl::Window>(); // TODO remove
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &io        = ImGui::GetIO();
      const auto &e_arcball = info.parent().relative("viewport_input_camera")("arcball").getr<detail::Arcball>();

      // Escape for empty scenes
      guard(!e_scene.components.objects.empty());

      // Compute viewport offset and size, minus ImGui's tab bars etc
      eig::Array2f viewport_offs = static_cast<eig::Array2f>(ImGui::GetWindowPos()) 
                                 + static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax())
                                 - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin());

      // Update pixel sensor
      m_query_sensor.proj_trf  = e_arcball.proj().matrix();
      m_query_sensor.view_trf  = e_arcball.view().matrix();
      m_query_sensor.film_size = viewport_size.cast<uint>();
      m_query_sensor.pixel     = eig::window_to_pixel(io.MousePos, viewport_offs, viewport_size);
      m_query_sensor.flush();
      
      // Perform path query and obtain path data
      auto &i_path_query = info("path_query").getw<PathQueryPrimitive>();
      i_path_query.query(m_query_sensor, e_scene, m_query_spp);
      auto paths = i_path_query.data();
      guard(!paths.empty());

      struct SeparationRecord {
        uint         power;  // nr. of times constriant reflectance appears along path
        eig::Array4f wvls;   // integration wavelengths 
        eig::Array4f values; // remainder of incident radiance, without constraint reflectance
      };

      // Integration color matching functions, s.t. a unit spectrum integrates to 1 luminance
      CMFS cmfs = ColrSystem { .cmfs = e_scene.primary_observer(), .illuminant = Spec(1) }.finalize();
      
      // Divider by nr. of requested path samples; not total paths. Most extra paths
      // come from NEE, and further division is taken into account by probability weighintg
      float colr_div = 1.f / (4.f * static_cast<float>(m_query_spp));
      colr_div *= wavelength_samples;

      // For each path, integrate spectral throughput into a distribution and
      // then convert this to a color.
      // Basically attempt to reproduce color output for testing
      Spec spec_distr = std::transform_reduce(std::execution::par_unseq,
        range_iter(paths), Spec(0), 
        [](const auto &a, const auto &b) -> Spec { return (a + b).eval(); }, 
        [colr_div](const auto &path) -> Spec { return colr_div * accumulate_spectrum(path.wvls, path.L);
      }).max(0.f).eval();
      Colr colr_lrgb_dstr = (cmfs.transpose() * spec_distr.matrix());
      Colr colr_srgb_dstr = lrgb_to_srgb(colr_lrgb_dstr);
      auto colr_luminance = luminance(colr_lrgb_dstr);
      
      // Attempt a reconstruction of the first vertex spectrum
      {
        ImGui::BeginTooltip();
    
        // Plot intergrated color
        ImGui::ColorEdit3("lrgb", colr_lrgb_dstr.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("srgb", colr_srgb_dstr.data(), ImGuiColorEditFlags_Float);
        ImGui::Value("Luminance", colr_luminance);

        // Run a spectrum plot for the accumulated radiance
        ImGui::Separator();
        ImGui::PlotSpectrum("##rad_plot", spec_distr, -0.05f, spec_distr.maxCoeff() + 0.05f, { -1, 96.f * e_window.content_scale() });
        
        ImGui::EndTooltip();
      }
    }

    void eval(SchedulerHandle &info) override {
      met_trace();
      
      bool is_open = true;
      if (ImGui::Begin("Path measure tool", &is_open)) {
        uint min_v = 0, max_v = 4096;
        ImGui::SliderScalar("Sample count", ImGuiDataType_U32, &m_query_spp, &min_v, &max_v);
      }
      ImGui::End();
      
      // Handle path queries, if m_query_spp != 0
      measure(info);

      // Window closed, kill this task
      if (!is_open)
        info.task().dstr();
    }
  };
} // namespace met