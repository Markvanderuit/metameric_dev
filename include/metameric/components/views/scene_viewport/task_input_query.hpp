#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/record.hpp>
#include <metameric/core/scene.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/components/views/detail/arcball.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <metameric/components/pipeline_new/task_gen_uplifting_data.hpp>
#include <metameric/render/primitives_query.hpp>
#include <small_gl/texture.hpp>
#include <small_gl/window.hpp>

// TODO remove
#include <algorithm>
#include <execution>
#include <implot.h>

namespace met {
  class MeshViewportQueryInputTask : public detail::TaskNode {
    PixelSensor m_query_sensor;
    uint        m_query_spp = 0;

  public:
    bool is_active(SchedulerHandle &info) override {
      return info.parent()("is_active").getr<bool>();
    }

    void init(SchedulerHandle &info) override {
      met_trace();
      info("path_query").init<PathQueryPrimitive>({
        .max_depth    = 4,
        .cache_handle = info.global("cache")
      });
    } 

    void eval_path_query(SchedulerHandle &info) {
      met_trace_full();

      // Get shared resources
      const auto &e_window  = info.global("window").getr<gl::Window>(); // TODO remove
      const auto &e_scene   = info.global("scene").getr<Scene>();
      const auto &io        = ImGui::GetIO();
      const auto &e_arcball = info.relative("viewport_input_camera")("arcball").getr<detail::Arcball>();
      auto &i_path_query    = info("path_query").getw<PathQueryPrimitive>();

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
      i_path_query.query(m_query_sensor, e_scene, m_query_spp);
      auto paths = i_path_query.data();
      guard(!paths.empty());

      struct SeparationRecord {
        uint         power;  // nr. of times constriant reflectance appears along path
        eig::Array4f wvls;   // integration wavelengths 
        eig::Array4f values; // remainder of incident radiance, without constraint reflectance
      };

      // Collect handles to all uplifting tasks
      std::vector<TaskHandle> uplf_handles;
      rng::transform(vws::iota(0u, static_cast<uint>(e_scene.components.upliftings.size())), 
        std::back_inserter(uplf_handles), [&](uint i) { return info.task(std::format("gen_upliftings.gen_uplifting_{}", i)); });

      // Integration color matching functions, s.t. a unit spectrum integrates to 1 luminance
      CMFS cmfs = ColrSystem {
        .cmfs       = e_scene.resources.observers[e_scene.components.observer_i.value].value(),
        .illuminant = Spec(1)
      }.finalize();

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
        [colr_div](const auto &path) -> Spec { return colr_div * accumulate_spectrum(path.wavelengths, path.L);
      }).max(0.f).eval();
      Colr colr_lrgb_dstr = (cmfs.transpose() * spec_distr.matrix());
      Colr colr_srgb_dstr = lrgb_to_srgb(colr_lrgb_dstr);
      
      // Assume for now, only one uplifting exists
      // Continue only if there is a constraint
      const auto &e_uplifting = e_scene.components.upliftings[0].value;
      guard(!e_uplifting.verts.empty());
      
      // Attempt a reconstruction of the first vertex spectrum
      {
        ImGui::BeginTooltip();
    
        // Plot intergrated color
        ImGui::ColorEdit3("lrgb", colr_lrgb_dstr.data(), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("srgb", colr_srgb_dstr.data(), ImGuiColorEditFlags_Float);

        ImGui::Separator();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45);
        ImGui::Value("Minimum", spec_distr.minCoeff());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45);
        ImGui::Value("Maximum", spec_distr.maxCoeff());

        ImGui::Separator();

        // Get wavelength values for x-axis in plot
        Spec x_values;
        rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());

        // Run a spectrum plot for the accumulated radiance
        if (ImPlot::BeginPlot("##rad_plot", { 256.f * e_window.content_scale(), 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
          // Setup minimal format for coming line plots
          ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
          ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, -0.05f, spec_distr.maxCoeff() + 0.05f, ImPlotCond_Always);

          // Iterate tetrahedron data and plot it
          ImPlot::PlotLine("##rad_line", x_values.data(), spec_distr.data(), wavelength_samples);

          ImPlot::EndPlot();
        }

        // Run a spectrum plot for encountered spectra
        /* if (ImPlot::BeginPlot("Reflectances", { 256.f * e_window.content_scale(), 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
          // Setup minimal format for coming line plots
          ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
          ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, -0.05, 1.05, ImPlotCond_Always);
          ImPlot::SetupAxisTicks(ImAxis_X1, nullptr, 0);
          ImPlot::SetupAxisTicks(ImAxis_Y1, nullptr, 0);

          // Iterate tetrahedron data and plot it
          uint scope_i = 0;
          for (const auto &data : tetr_data) {
            for (const auto &tetr : data) {
              Spec r = tetr.weights[0] * tetr.spectra[0] + tetr.weights[1] * tetr.spectra[1]
                     + tetr.weights[2] * tetr.spectra[2] + tetr.weights[3] * tetr.spectra[3];
              ImPlot::PlotLine(std::format("##scope_{}", scope_i++).c_str(), x_values.data(), r.data(), wavelength_samples);
            }
          }

          ImPlot::EndPlot();
        } */
        
        ImGui::EndTooltip();
      }
    }

    void eval(SchedulerHandle &info) override {
      met_trace();
      
      if (ImGui::Begin("Blahhh")) {
        uint min_v = 0, max_v = 4096;
        ImGui::SliderScalar("Slider", ImGuiDataType_U32, &m_query_spp, &min_v, &max_v);
      }
      ImGui::End();
      
      if (m_query_spp > 0) {
        eval_path_query(info);
      }
    }
  };
} // namespace met