#include <metameric/components/tasks/task_gen_spectral_gamut.hpp>
#include <metameric/core/detail/trace.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <algorithm>
#include <execution>
#include <ranges>

namespace met {
  GenSpectralGamutTask::GenSpectralGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralGamutTask::init(detail::TaskInitInfo &info) {
    met_trace_full();

    // Define flags for creation of a persistent, write-only flushable buffer map
    auto create_flags = gl::BufferCreateFlags::eMapWrite 
                      | gl::BufferCreateFlags::eMapPersistent;
    auto map_flags    = gl::BufferAccessFlags::eMapWrite 
                      | gl::BufferAccessFlags::eMapPersistent
                      | gl::BufferAccessFlags::eMapFlush;
    
    // Define color and spectral gamut buffers
    gl::Buffer color_buffer = {{ .size  = 4 * sizeof(AlColr), .flags = create_flags }};
    gl::Buffer spect_buffer = {{ .size  = 4 * sizeof(Spec), .flags = create_flags }};

    // Prepare buffer maps which are written to **every** frame
    auto color_buffer_map = cast_span<AlColr>(color_buffer.map(map_flags));
    auto spect_buffer_map = cast_span<Spec>(spect_buffer.map(map_flags));

    // Submit resources 
    info.insert_resource("color_buffer", std::move(color_buffer));
    info.insert_resource("spectrum_buffer", std::move(spect_buffer));
    info.insert_resource("color_buffer_map", std::move(color_buffer_map));
    info.insert_resource("spectrum_buffer_map", std::move(spect_buffer_map));
  }

  void GenSpectralGamutTask::dstr(detail::TaskDstrInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &i_color_buffer = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");

    // Unmap buffers
    i_color_buffer.unmap();
    i_spect_buffer.unmap();
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    met_trace_full();

    // Get shared resources
    auto &e_app_data         = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_color_buffer     = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_buffer     = info.get_resource<gl::Buffer>("spectrum_buffer");
    auto &i_color_buffer_map = info.get_resource<std::span<AlColr>>("color_buffer_map");
    auto &i_spect_buffer_map = info.get_resource<std::span<Spec>>("spectrum_buffer_map");

    // Get relevant application/project data
    auto &knn_grid    = e_app_data.spec_knn_grid;
    auto &color_gamut = e_app_data.project_data.rgb_gamut;
    auto &spect_gamut = e_app_data.project_data.spec_gamut;
      
    // Sample spectra at gamut color positions from the KNN object
    std::transform(std::execution::par_unseq, range_iter(color_gamut), spect_gamut.begin(),
      [&](const auto &p) { return knn_grid.query_1_nearest(p).value; });
    
    // Copy data to gpu buffer maps
    std::ranges::copy(color_gamut, i_color_buffer_map.begin());
    std::ranges::copy(spect_gamut, i_spect_buffer_map.begin());

    // Flush buffers after pushing new data 
    i_color_buffer.flush();
    i_spect_buffer.flush();
  }
} // namespace met
