#pragma once

#include <metameric/core/scheduler.hpp>
#include <metameric/core/ray.hpp>
#include <metameric/core/scene.hpp>
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
      info("path_query").init<FullPathQueryPrimitive>({
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
      auto &i_path_query    = info("path_query").getw<FullPathQueryPrimitive>();

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
      auto _paths = i_path_query.data();
      std::vector<PathRecord> paths(range_iter(_paths));

      guard(!paths.empty());

      // Collect handles to all uplifting tasks
      std::vector<TaskHandle> uplf_handles;
      rng::transform(vws::iota(0u, static_cast<uint>(e_scene.components.upliftings.size())), 
        std::back_inserter(uplf_handles), [&](uint i) { return info.task(std::format("gen_upliftings.gen_uplifting_{}", i)); });

      // Camera cmfs
      CMFS cmfs = e_scene.resources.observers[e_scene.components.observer_i.value].value();
      cmfs = (cmfs.array())
           / (cmfs.array().col(1) * wavelength_ssize).sum();
      cmfs = (models::xyz_to_srgb_transform * cmfs.matrix().transpose()).transpose();

      // For each path, sum spectral information into a relevant color;
      // basically attempt to reproduce output color for testing
      float colr_div = 1.f / static_cast<float>(m_query_spp);
      Colr colr_lrgb = std::transform_reduce(std::execution::par_unseq,
        range_iter(paths), Colr(0), 
        [](const auto &a, const auto &b) -> Colr { 
          return (a + b).eval(); }, 
        [&cmfs, colr_div](const auto &path) -> Colr {
          return colr_div * integrate_cmfs(cmfs, path.wavelengths, path.L);
      });
      Colr colr_srgb = lrgb_to_srgb(colr_lrgb);

      // For each path, gather relevant spectral tetrahedron data along vertices
      std::vector<std::array<TetrahedronRecord, PathRecord::path_max_depth>> tetr_data(paths.size());
      std::transform(std::execution::par_unseq, range_iter(paths), tetr_data.begin(), [&](const PathRecord &record) {
        std::array<TetrahedronRecord, PathRecord::path_max_depth> data;
        data.fill(TetrahedronRecord { .weights = { .25f, .25f, .25f, .25f }, .verts   = { 0.f, 0.f, 0.f, 0.f },
                                      .spectra = { 1.f, 1.f, 1.f, 1.f     }, .indices = { -1, -1, -1, -1     }});

        // We take n-1 vertices, as the last vertex necessarily hits an emitter
        int n_refl_vertices = std::clamp(static_cast<int>(record.path_depth) - 1, // last one is an emitter, which we do not care about
                                         0,
                                         static_cast<int>(PathRecord::path_max_depth) - 1);

        // For each vertex, find reflectance data 
        rng::transform(record.data | vws::take(n_refl_vertices), data.begin(), [&](const auto &vert) {
          // Query surface info at the vertex position
          SurfaceInfo si   = e_scene.get_surface_info(vert.p, vert.record);

          // Get handle to relevant uplifting task and generate a uplifting tetrahedron that constructs the surface reflectance
          uint uplifting_i = e_scene.components.objects[si.record.object_i()].value.uplifting_i;
          return uplf_handles[uplifting_i].realize<GenUpliftingDataTask>().query_tetrahedron(si.diffuse);
        });
        
        return data;
      });
      
      // Attempt a reconstruction of the first vertex spectrum
      if (paths[0].path_depth > 1) {
        ImGui::BeginTooltip();
        
        ImGui::ColorEdit3("##color", colr_srgb.data(), ImGuiColorEditFlags_Float);

        if (ImPlot::BeginPlot("##output_spectrum_plot", { 256.f * e_window.content_scale(), 128.f * e_window.content_scale() }, ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) {
          // Get wavelength values for x-axis in plot
          Spec x_values;
          rng::copy(vws::iota(0u, wavelength_samples) | vws::transform(wavelength_at_index), x_values.begin());

          // Setup minimal format for coming line plots
          ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
          ImPlot::SetupAxesLimits(wavelength_min, wavelength_max, -0.05, 1.05, ImPlotCond_Always);
          ImPlot::SetupAxisTicks(ImAxis_X1, nullptr, 0);
          ImPlot::SetupAxisTicks(ImAxis_Y1, nullptr, 0);
          
          for (int i = 0; i < static_cast<int>(paths[0].path_depth) - 1; ++i) {
            auto tetr = tetr_data[0][i];
            Spec r = tetr.weights[0] * tetr.spectra[0]
                   + tetr.weights[1] * tetr.spectra[1]
                   + tetr.weights[2] * tetr.spectra[2]
                   + tetr.weights[3] * tetr.spectra[3];

            // Do the thing
            ImPlot::PlotLine(std::format("{}", i).c_str(), x_values.data(), r.data(), wavelength_samples);
          }

          ImPlot::EndPlot();
        }
        ImGui::EndTooltip();
      }
    }

    void eval(SchedulerHandle &info) override {
      met_trace();
      
      if (ImGui::Begin("Blahhh")) {
        uint min_v = 0, max_v = 1024;
        ImGui::SliderScalar("Slider", ImGuiDataType_U32, &m_query_spp, &min_v, &max_v);
      }
      ImGui::End();
      
      if (m_query_spp > 0) {
        eval_path_query(info);
      }
    }
  };
} // namespace met