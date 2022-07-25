#include <metameric/components/tasks/generate_gamut.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/state.hpp>
#include <small_gl/buffer.hpp>
#include <ranges>

namespace met {
  GenerateGamutTask::GenerateGamutTask(const std::string &name)
  : detail::AbstractTask(name) { }
  
  void GenerateGamutTask::init(detail::TaskInitInfo &info) {
    // Submit resources: buffers storing the four colours/spectra forming a tetrahedron
    info.emplace_resource<gl::Buffer>("color_gamut_buffer", {
      .size  = sizeof(eig::AlArray3f) * 4, 
      .flags = gl::BufferCreateFlags::eMapReadWrite | gl::BufferCreateFlags::eStorageDynamic
    });
    info.emplace_resource<gl::Buffer>("spectral_gamut_buffer", {
      .size  = sizeof(Spec) * 4, 
      .flags = gl::BufferCreateFlags::eMapReadWrite | gl::BufferCreateFlags::eStorageDynamic
    });
  }
  
  void GenerateGamutTask::eval(detail::TaskEvalInfo &info) {
    // Get shared resources
    auto &e_app_data          = info.get_resource<ApplicationData>("global", "application_data");
    auto &i_colr_gamut_buffer = info.get_resource<gl::Buffer>("color_gamut_buffer");
    auto &i_spec_gamut_buffer = info.get_resource<gl::Buffer>("spectral_gamut_buffer");

    // Get relevant application data
    std::array<Color, 4> &rgb_gamut = e_app_data.project_data.rgb_gamut;
    std::array<Spec, 4> &spec_gamut = e_app_data.project_data.spec_gamut;
      
    // Sample spectra at gamut positions from KNN object
    std::ranges::transform(rgb_gamut, spec_gamut.begin(),
      [&](const auto &p) { return e_app_data.spec_knn_grid.query_1_nearest(p).value; });
    
    // Copy over gamut data to aligned type
    std::array<eig::AlArray3f, 4> rgb_gamut_aligned;
    std::ranges::copy(rgb_gamut, rgb_gamut_aligned.begin());

    // Copy over data
    i_colr_gamut_buffer.set(as_span<std::byte>(rgb_gamut_aligned));
    i_spec_gamut_buffer.set(as_span<std::byte>(spec_gamut));
  }
} // namespace met
