#include <metameric/core/spectrum.hpp>
#include <array>
#include <algorithm>
#include <ranges>

namespace met {
  namespace models {
    namespace detail {
      #include <metameric/core/detail/spectrum_models_cie.ext>

      CMFS load_cmfs_xyz() {
        CMFS c;
        
        for (size_t i = 0; i < wavelength_samples; ++i) {
          // Compute the indices to sample cmfs_xyz_values_*, and a mixing 
          // alpha in case our wavelengths are not perfectly aligned
          float offset = wavelength_at_index(i) - cie_wavelength_min,
                alpha  = offset - static_cast<float>(std::floor(offset));
          size_t j_min = static_cast<size_t>(std::floor(offset)),
                j_max = static_cast<size_t>(std::ceil(offset));

          // Obtain vectors by sampling the cmfs_xyz_values_* at the right positions
          eig::Vector3f c_min { cie_xyz_values_x[j_min], cie_xyz_values_y[j_min], cie_xyz_values_z[j_min] };
          eig::Vector3f c_max { cie_xyz_values_x[j_max], cie_xyz_values_y[j_max], cie_xyz_values_z[j_max] };

          // The actual value is a mixture, as the sampled wavelengths might not
          // align perfectly with the cmfs_xyz_values_* array's values
          c.col(i) = c_min + (offset - static_cast<float>(j_min)) * (c_max - c_min);
        }
        
        return c;
      }
    } // namespace detail

    // Load color matching functions and SPD models
    CMFS cmfs_cie_xyz = detail::load_cmfs_xyz();
    Spectrum emitter_cie_d65 = spectrum_from_data(detail::cie_wavelength_values, 
                                                  detail::cie_d65_values);
    Spectrum emitter_cie_e = 1.f;
  } // namespace models 
  
  CMFS cmfs_from_data(std::span<const float> wvls, 
                      std::span<const float> values_x,
                      std::span<const float> values_y,
                      std::span<const float> values_z) {
    CMFS c = 0.f;
    Spectrum n = 0.f;

    // Add values into the CMFS object at their respective wavelength bin
    for (size_t i = 0; i < wvls.size(); ++i) {
      float wvl = wvls[i];
      guard_continue(wvl >= wavelength_min && wvl <= wavelength_max);

      size_t index = index_at_wavelength(wvl);
      c.col(index) += eig::Vector3f { values_x[i], values_y[i], values_z[i] };
      n[index]++;
    }
    
    // Average each bin for the nr. of provided values
    for (auto row : c.rowwise()) {
      row.array() /= n.max(1.f);
    }
    
    return c;
  }

  Spectrum spectrum_from_data(std::span<const float> wvls,
                              std::span<const float> values) {
    Spectrum s = 0.f, n = 0.f;

    // Add values into the Spectrum object at their respective wavelength bin
    for (size_t i = 0; i < wvls.size(); ++i) {
      float wvl = wvls[i];
      guard_continue(wvl >= wavelength_min && wvl <= wavelength_max);

      size_t index = index_at_wavelength(wvl);
      s[index] += values[i];
      n[index]++;
    }

    // Average each bin for the nr. of provided values
    s = s / n.max(1.f);

    return s;
  }
} // namespace met