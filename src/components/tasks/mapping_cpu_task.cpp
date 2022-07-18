#include <metameric/core/io.hpp>
#include <metameric/core/knn.hpp>
#include <metameric/components/tasks/mapping_cpu_task.hpp>
#include <metameric/components/views/detail/imgui.hpp>
#include <small_gl/buffer.hpp>
#include <fmt/ranges.h>
#include <execution>
#include <numeric>
#include <ranges>

namespace met {
  namespace detail {
    eig::Vector4f as_barycentric(std::span<Color> gamut, const Color &p) {
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
    auto &e_texture_obj = info.get_resource<io::TextureData<Color>>("global", "color_texture_buffer_cpu");
    
    // Fill input texture with data 
    m_input = std::vector<eig::Array3f>(e_texture_obj.data.begin(), e_texture_obj.data.end());

    // Set up processing buffers on the CPU
    size_t texture_size = glm::prod(e_texture_obj.size);
    m_barycentric_texture.resize(texture_size);
    m_spectral_texture.resize(texture_size);
    m_output_d65.resize(texture_size);
    m_output_d65_err.resize(texture_size);
    m_output_fl2.resize(texture_size);
    m_output_fl11.resize(texture_size);

    // Set up view texture on the GPU
    m_input_texture          = {{ .size = e_texture_obj.size, .data = as_typed_span<float>(m_input) }};
    m_output_d65_texture     = {{ .size = e_texture_obj.size }};
    m_output_d65_err_texture = {{ .size = e_texture_obj.size }};
    m_output_fl2_texture     = {{ .size = e_texture_obj.size }};
    m_output_fl11_texture    = {{ .size = e_texture_obj.size }};
  }

  void MappingCPUTask::eval(detail::TaskEvalInfo &info) {
    if (ImGui::Begin("CPU mapping")) {
      // Get externally shared resources
      auto &e_spectral_knn_grid  = info.get_resource<KNNGrid<Spec>>("global", "spectral_knn_grid");
      auto &e_color_gamut_buffer = info.get_resource<gl::Buffer>("global", "color_gamut_buffer");

      // Generate temporary mapping to color gamut buffer 
      auto color_gamut_map = convert_span<eig::AlArray3f>(e_color_gamut_buffer.map(gl::BufferAccessFlags::eMapReadWrite));
      
      // Sample spectra at gamut corner positions
      std::vector<Spec> spectral_gamut(4);
      std::vector<Color> color_gamut(4);
      std::ranges::copy(color_gamut_map, color_gamut.begin());
      std::ranges::transform(color_gamut, spectral_gamut.begin(),
      [&](const auto &p) { return e_spectral_knn_grid.query_1_nearest(p).value; });
        
      // Close buffer mapping (should wrap this in a scoped object)
      e_color_gamut_buffer.unmap();
      
      // Generate barycentric coordinate for all colors
      std::ranges::transform(m_input, m_barycentric_texture.begin(),
        [&](const auto &p) { return detail::as_barycentric(color_gamut, p); });

      // Generate high-dimensional spectral texture
      std::transform(std::execution::par_unseq, 
        m_barycentric_texture.begin(), m_barycentric_texture.end(), m_spectral_texture.begin(),
        [&](const eig::Vector4f &abcd) {
          return spectral_gamut[0] * abcd.x() + spectral_gamut[1] * abcd.y()
               + spectral_gamut[2] * abcd.z() + spectral_gamut[3] * abcd.w();
        });

      // Generate and output average spectrum
      Spec test_spectrum = m_spectral_texture[1024];
      //  std::reduce(std::execution::par_unseq, m_spectral_texture.begin(), 
        // m_spectral_texture.end()) / static_cast<float>(m_spectral_texture.size());
      Color test_color = reflectance_to_color(test_spectrum, { 
        .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_fl2
      });

      // Specify spectrum-to-color mappings
      const SpectralMapping mapping_d65  = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_d65  };
      const SpectralMapping mapping_fl2  = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_fl2  };
      const SpectralMapping mapping_fl11 = { .cmfs = models::cmfs_srgb, .illuminant = models::emitter_cie_fl11 };

      // Generate low-dimensional color texture
      std::transform(std::execution::par_unseq, 
        m_spectral_texture.begin(), m_spectral_texture.end(), m_output_d65.begin(), 
        [&](const Spec &sd) { return mapping_d65.apply(sd); });
      std::transform(std::execution::par_unseq, 
        m_spectral_texture.begin(), m_spectral_texture.end(), m_output_fl2.begin(), 
        [&](const Spec &sd) { return mapping_fl2.apply(sd); });
      std::transform(std::execution::par_unseq, 
        m_spectral_texture.begin(), m_spectral_texture.end(), m_output_fl11.begin(), 
        [&](const Spec &sd) { return mapping_fl11.apply(sd); });

      #pragma omp parallel for
      for (uint i = 0; i < m_output_d65.size(); ++i) {
        Color a = m_output_d65[i], 
              b = m_input[i];
        m_output_d65_err[i] = Color((b - a).square().sum()); // squared error
      }
      Color output_d65_mse = std::reduce(m_output_d65_err.begin(), m_output_d65_err.end())
                           / static_cast<float>(m_output_d65_err.size());

      // Copy data into view texture
      m_output_d65_texture.set(as_typed_span<float>(m_output_d65));
      m_output_d65_err_texture.set(as_typed_span<float>(m_output_d65_err));
      m_output_fl2_texture.set(as_typed_span<float>(m_output_fl2));
      m_output_fl11_texture.set(as_typed_span<float>(m_output_fl11));

      // Show texture
      auto viewport_size = static_cast<glm::vec2>(ImGui::GetWindowContentRegionMax().x)
                         - static_cast<glm::vec2>(ImGui::GetWindowContentRegionMin().x);
      auto texture_aspect = static_cast<float>(m_output_d65_texture.size().y) 
                          / static_cast<float>(m_output_d65_texture.size().x);
      
      // Draw RGB texture
      ImGui::BeginGroup();
      ImGui::Text("RGB input");
      ImGui::Image(ImGui::to_ptr(m_input_texture.object()), viewport_size * glm::vec2(0.45, 0.45 * texture_aspect));
      ImGui::EndGroup();

      ImGui::SameLine();

      // Draw D65 uplifted texture
      ImGui::BeginGroup();
      ImGui::Text("D65 output");
      ImGui::Image(ImGui::to_ptr(m_output_d65_texture.object()), viewport_size * glm::vec2(0.45, 0.45 * texture_aspect));
      ImGui::EndGroup();

      /* Begin new line */
      ImGui::Separator();

      ImGui::BeginGroup();
      ImGui::Text("Mean reflectance");
      ImGui::PlotLines("", test_spectrum.data(), wavelength_samples, 0,
        nullptr, 0.f, 1.f, viewport_size * glm::vec2(0.45, 0.45 * texture_aspect));
      ImGui::EndGroup();

      ImGui::SameLine();

      ImGui::BeginGroup();
      ImGui::Text("D65, squared err.");
      ImGui::Image(ImGui::to_ptr(m_output_d65_err_texture.object()), viewport_size * glm::vec2(0.45, 0.45 * texture_aspect));
      ImGui::Value("D65, mean squared err.", output_d65_mse.x(), "%.5f");
      ImGui::EndGroup();
      
      /* Begin new line */
      ImGui::Separator();

      ImGui::BeginGroup();
      ImGui::Text("FL2 output");
      ImGui::Image(ImGui::to_ptr(m_output_fl2_texture.object()), viewport_size * glm::vec2(0.45, 0.45 * texture_aspect));
      ImGui::EndGroup();

      ImGui::SameLine();

      ImGui::BeginGroup();
      ImGui::Text("FL11 output");
      ImGui::Image(ImGui::to_ptr(m_output_fl11_texture.object()), viewport_size * glm::vec2(0.45, 0.45 * texture_aspect));
      ImGui::EndGroup();
    }
    ImGui::End();
  }
} // namespace met