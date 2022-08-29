#include <metameric/core/knn.hpp>
#include <metameric/core/state.hpp>
#include <metameric/core/texture.hpp>
#include <metameric/components/tasks/mapping_cpu_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <execution>
#include <numeric>
#include <ranges>

namespace met {
  namespace detail {
    eig::Vector4f as_barycentric(std::span<Colr> gamut, const Colr &p) {
      auto t = (eig::Matrix3f() << (gamut[0] - gamut[3]),
                                   (gamut[1] - gamut[3]),
                                   (gamut[2] - gamut[3])).finished();
      auto abc = t.inverse() * (p - gamut[3]).matrix();
      return (eig::Vector4f() << abc, 1.f - abc.sum()).finished();
    }
  } // namespace detail
  
  MappingCPUTask::MappingCPUTask(const std::string &name)
  : detail::AbstractTask(name) { }

  void MappingCPUTask::init(detail::TaskInitInfo &info) {
    // Get externally shared resources
    auto &e_rgb_texture = info.get_resource<ApplicationData>(global_key, "app_data").loaded_texture;
    
    // Fill input texture with data 
    m_input = std::vector<eig::Array3f>(range_iter(e_rgb_texture.data()));

    // Set up processing buffers on the CPU
    size_t texture_size = e_rgb_texture.size().prod();
    m_barycentric_texture.resize(texture_size);
    m_spectral_texture.resize(texture_size);
    m_output_d65.resize(texture_size);
    m_output_d65_err.resize(texture_size);
    m_output_fl2.resize(texture_size);
    m_output_fl11.resize(texture_size);

    // Set up view texture on the GPU
    m_input_texture          = {{ .size = e_rgb_texture.size(), .data = cast_span<float>(e_rgb_texture.data()) }};
    m_output_d65_texture     = {{ .size = e_rgb_texture.size() }};
    m_output_d65_err_texture = {{ .size = e_rgb_texture.size() }};
    m_output_fl2_texture     = {{ .size = e_rgb_texture.size() }};
    m_output_fl11_texture    = {{ .size = e_rgb_texture.size() }};
  }

  void MappingCPUTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("CPU mapping")) {
      // Get externally shared resources
      auto &e_app_data = info.get_resource<ApplicationData>(global_key, "app_data");

      // Get relevant application data
      std::array<Colr, 4> &rgb_gamut = e_app_data.project_data.rgb_gamut;
      std::array<Spec, 4> &spec_gamut = e_app_data.project_data.spec_gamut;
      
      // Generate barycentric coordinate for all colors
      std::ranges::transform(m_input, m_barycentric_texture.begin(),
        [&](const auto &p) { return detail::as_barycentric(rgb_gamut, p); });

      // Generate high-dimensional spectral texture
      std::transform(std::execution::par_unseq, 
        m_barycentric_texture.begin(), m_barycentric_texture.end(), m_spectral_texture.begin(),
        [&](const eig::Vector4f &abcd) {
          return spec_gamut[0] * abcd.x() + spec_gamut[1] * abcd.y()
               + spec_gamut[2] * abcd.z() + spec_gamut[3] * abcd.w();
        });

      // Generate and output average spectrum
      Spec test_spectrum = m_spectral_texture[1024];
      //  std::reduce(std::execution::par_unseq, m_spectral_texture.begin(), 
        // m_spectral_texture.end()) / static_cast<float>(m_spectral_texture.size());
      Colr test_color = reflectance_to_color(test_spectrum, { 
        .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_fl2
      });

      // Specify spectrum-to-color mappings
      const SpectralMapping mapping_d65  = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_d65,  .n_scatters = 0 };
      const SpectralMapping mapping_fl2  = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_fl2,  .n_scatters = 1 };
      const SpectralMapping mapping_fl11 = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_fl11, .n_scatters = 1 };

      // Generate low-dimensional color texture
      std::transform(std::execution::par_unseq, 
        m_spectral_texture.begin(), m_spectral_texture.end(), m_output_d65.begin(), 
        [&](const Spec &sd) { return mapping_d65.apply_color(sd); });
      std::transform(std::execution::par_unseq, 
        m_spectral_texture.begin(), m_spectral_texture.end(), m_output_fl2.begin(), 
        [&](const Spec &sd) { return mapping_fl2.apply_color(sd); });
      std::transform(std::execution::par_unseq, 
        m_spectral_texture.begin(), m_spectral_texture.end(), m_output_fl11.begin(), 
        [&](const Spec &sd) { return mapping_fl11.apply_color(sd); });

      #pragma omp parallel for
      for (int i = 0; i < static_cast<int>(m_output_d65.size()); ++i) {
        Colr a = m_output_d65[i], b = m_input[i];
        m_output_d65_err[i] = Colr((b - a).square().sum()); // squared error
      }
      Colr output_d65_mse = std::reduce(m_output_d65_err.begin(), m_output_d65_err.end())
                           / static_cast<float>(m_output_d65_err.size());

      // Copy data into view texture
      m_output_d65_texture.set(as_span<float>(m_output_d65));
      m_output_d65_err_texture.set(as_span<float>(m_output_d65_err));
      m_output_fl2_texture.set(as_span<float>(m_output_fl2));
      m_output_fl11_texture.set(as_span<float>(m_output_fl11));

      // Show texture
      eig::Array2f viewport_size = static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMax().x)
                                - static_cast<eig::Array2f>(ImGui::GetWindowContentRegionMin().x);
      auto texture_aspect = static_cast<float>(m_output_d65_texture.size().y()) 
                          / static_cast<float>(m_output_d65_texture.size().x());
      
      // Draw RGB texture
      ImGui::BeginGroup();
      ImGui::Text("RGB input");
      ImGui::Image(ImGui::to_ptr(m_input_texture.object()), (viewport_size * eig::Array2f(0.45, 0.45 * texture_aspect)).eval());
      ImGui::EndGroup();

      ImGui::SameLine();

      // Draw D65 uplifted texture
      ImGui::BeginGroup();
      ImGui::Text("D65 output");
      ImGui::Image(ImGui::to_ptr(m_output_d65_texture.object()), (viewport_size * eig::Array2f(0.45, 0.45 * texture_aspect)).eval());
      ImGui::EndGroup();

      /* Begin new line */
      ImGui::Separator();

      ImGui::BeginGroup();
      ImGui::Text("Mean reflectance");
      ImGui::PlotLines("", test_spectrum.data(), wavelength_samples, 0,
        nullptr, 0.f, 1.f, (viewport_size * eig::Array2f(0.45, 0.45 * texture_aspect)).eval());
      ImGui::EndGroup();

      ImGui::SameLine();

      ImGui::BeginGroup();
      ImGui::Text("D65, squared err.");
      ImGui::Image(ImGui::to_ptr(m_output_d65_err_texture.object()), (viewport_size * eig::Array2f(0.45, 0.45 * texture_aspect)).eval());
      ImGui::Value("D65, mean squared err.", output_d65_mse.x(), "%.5f");
      ImGui::EndGroup();
      
      /* Begin new line */
      ImGui::Separator();

      ImGui::BeginGroup();
      ImGui::Text("FL2 output");
      ImGui::Image(ImGui::to_ptr(m_output_fl2_texture.object()), (viewport_size * eig::Array2f(0.45, 0.45 * texture_aspect)).eval());
      ImGui::EndGroup();

      ImGui::SameLine();

      ImGui::BeginGroup();
      ImGui::Text("FL11 output");
      ImGui::Image(ImGui::to_ptr(m_output_fl11_texture.object()), (viewport_size * eig::Array2f(0.45, 0.45 * texture_aspect)).eval());
      ImGui::EndGroup();
    }
    ImGui::End();
  }
} // namespace met