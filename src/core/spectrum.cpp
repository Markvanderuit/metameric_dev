#include <metameric/core/spectrum.hpp>
#include <array>
#include <algorithm>
#include <ranges>

namespace met {
  // Load color matching functions and SPD models
  namespace models {
    #include <metameric/core/detail/spectrum_models_cie.ext>

    Spectrum emitter_cie_d65 = spectrum_from_data(cie_wavelength_values, cie_d65_values);
    Spectrum emitter_cie_e   = 1.f;
    CMFS cmfs_cie_xyz        = cmfs_from_data(cie_wavelength_values, cie_xyz_values_x, 
                                              cie_xyz_values_y, cie_xyz_values_z);
  } // namespace models 

  // Src: Mitsuba 0.5, reimplements InterpolatedSpectrum::eval(...) from libcore/spectrum.cpp
  Spectrum spectrum_from_data(std::span<const float> wvls,
                              std::span<const float> values) {
    float data_wvl_min = wvls[0],
          data_wvl_max = wvls[wvls.size() - 1];

    Spectrum s = 0.f;
    for (size_t i = 0; i < wavelength_samples; ++i) {
      float spectrum_wvl_min = i * wavelength_sample_size + wavelength_min,
            spectrum_wvl_max = spectrum_wvl_min + wavelength_sample_size;

      // Determine accessible range of wavelengths
      float wvl_min = std::max(spectrum_wvl_min, data_wvl_min),
            wvl_max = std::min(spectrum_wvl_max, data_wvl_max);
      guard_continue(wvl_max > wvl_min);

      // Find the starting index using binary search (Thanks for the idea, Mitsuba people!)
      ptrdiff_t pos = std::max(std::ranges::lower_bound(wvls, wvl_min) - wvls.begin(),
                               static_cast<ptrdiff_t>(1)) - 1;
      
      // Step through the provided data and integrate trapezoids
      for (; pos + 1 < wvls.size() && wvls[pos] < wvl_max; ++pos) {
        float wvl_a   = wvls[pos],
              value_a = values[pos],
              clamp_a = std::max(wvl_a, wvl_min);
        float wvl_b   = wvls[pos + 1],
              value_b = values[pos + 1],
              clamp_b = std::min(wvl_b, wvl_max);
        guard_continue(clamp_b > clamp_a);

        float inv_ab = 1.f / (wvl_b - wvl_a);
        float interp_a = std::lerp(value_a, value_b, (clamp_a - wvl_a) * inv_ab),
              interp_b = std::lerp(value_a, value_b, (clamp_b - wvl_a) * inv_ab);

        s[i] += .5f * (interp_a + interp_b) * (clamp_b - clamp_a);
      }
      s[i] /= wavelength_sample_size;
    }

    return s.eval();
  }
  
  CMFS cmfs_from_data(std::span<const float> wvls, 
                      std::span<const float> values_x,
                      std::span<const float> values_y,
                      std::span<const float> values_z) {
    CMFS c;

    c.row(0) = spectrum_from_data(wvls, values_x);
    c.row(1) = spectrum_from_data(wvls, values_y);
    c.row(2) = spectrum_from_data(wvls, values_z);

    return c.eval();
  }
} // namespace met