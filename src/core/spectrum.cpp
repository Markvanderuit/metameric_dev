#include <metameric/core/spectrum.hpp>
#include <array>
#include <algorithm>
#include <ranges>

namespace met {
  namespace io {
    // Src: Mitsuba 0.5, reimplements InterpolatedSpectrum::eval(...) from libcore/spectrum.cpp
    Spec spectrum_from_data(std::span<const float> wvls, std::span<const float> values) {
      float data_wvl_min = wvls[0],
            data_wvl_max = wvls[wvls.size() - 1];

      Spec s = 0.f;
      for (size_t i = 0; i < wavelength_samples; ++i) {
        float spec_wvl_min = i * wavelength_ssize + wavelength_min,
              spec_wvl_max = spec_wvl_min + wavelength_ssize;

        // Determine accessible range of wavelengths
        float wvl_min = std::max(spec_wvl_min, data_wvl_min),
              wvl_max = std::min(spec_wvl_max, data_wvl_max);
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
        s[i] /= wavelength_ssize;
      }

      return s.eval();
    }
    
    CMFS cmfs_from_data(std::span<const float> wvls, std::span<const float> values_x,
                        std::span<const float> values_y, std::span<const float> values_z) {
      return (CMFS() << spectrum_from_data(wvls, values_x),
                        spectrum_from_data(wvls, values_y),
                        spectrum_from_data(wvls, values_z)).finished();
    }
  } // namepsace io

  namespace models {
    #include <metameric/core/detail/spectrum_models.ext>
    
    // Linear color space transformations
    eig::Matrix3f xyz_to_srgb_transform {{ 3.240479f, -1.537150f,-0.498535f },
                                         {-0.969256f,  1.875991f, 0.041556f },
                                         { 0.055648f, -0.204043f, 1.057311f }};
    eig::Matrix3f srgb_to_xyz_transform {{ 0.412453f, 0.357580f, 0.180423f },
                                         { 0.212671f, 0.715160f, 0.072169f },
                                         { 0.019334f, 0.119193f, 0.950227f }};

    // Color matching functions
    CMFS cmfs_cie_xyz = io::cmfs_from_data(cie_wavelength_values, cie_xyz_values_x, cie_xyz_values_y, cie_xyz_values_z);
    CMFS cmfs_srgb    = (xyz_to_srgb_transform * cmfs_cie_xyz.transpose()).transpose();

    // Illuminant spectra
    Spec emitter_cie_e       = 1.f;
    Spec emitter_cie_d65     = io::spectrum_from_data(cie_wavelength_values, cie_d65_values);
    Spec emitter_cie_fl2     = io::spectrum_from_data(cie_wavelength_values, cie_fl2_values);
    Spec emitter_cie_fl11    = io::spectrum_from_data(cie_wavelength_values, cie_fl11_values);
    Spec emitter_cie_ledb1   = io::spectrum_from_data(cie_wavelength_values, cie_ledb1_values);
    Spec emitter_cie_ledrgb1 = io::spectrum_from_data(cie_wavelength_values, cie_ledrgb1_values);
  } // namespace models 
} // namespace met
