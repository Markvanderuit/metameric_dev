#include <metameric/components/tasks/task_gen_spectral_gamut.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  GenSpectralGamutTask::GenSpectralGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenSpectralGamutTask::init(detail::TaskInitInfo &info) {
    // Submit resources: buffers storing the four colours/spectra forming a tetrahedron
    info.emplace_resource<gl::Buffer>("color_buffer", {
      .size  = 4 * sizeof(eig::AlArray3f),
      .flags = gl::BufferCreateFlags::eMapReadWrite | gl::BufferCreateFlags::eStorageDynamic
    });
    info.emplace_resource<gl::Buffer>("spectrum_buffer", {
      .size  = 4 * sizeof(Spec),
      .flags = gl::BufferCreateFlags::eMapReadWrite | gl::BufferCreateFlags::eStorageDynamic
    });
  }
  
  void GenSpectralGamutTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_app_data     = info.get_resource<ApplicationData>(global_key, "app_data");
    auto &i_color_buffer = info.get_resource<gl::Buffer>("color_buffer");
    auto &i_spect_buffer = info.get_resource<gl::Buffer>("spectrum_buffer");

    // Get relevant application/project data
    auto &knn_grid    = e_app_data.spec_knn_grid;
    auto &color_gamut = e_app_data.project_data.rgb_gamut;
    auto &spect_gamut = e_app_data.project_data.spec_gamut;
      
    // Sample spectra at gamut color positions from the KNN object
    std::ranges::transform(color_gamut, spect_gamut.begin(),
      [&](const auto &p) { return knn_grid.query_1_nearest(p).value; });
    
    // Copy over gamut data to aligned type
    std::array<eig::AlArray3f, 4> al_color_gamut;
    std::ranges::copy(color_gamut, al_color_gamut.begin());

    // Copy data to gpu buffers
    i_color_buffer.set(as_span<std::byte>(al_color_gamut));
    i_spect_buffer.set(as_span<std::byte>(spect_gamut));
  }
} // namespace met
