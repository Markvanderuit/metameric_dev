#pragma once

#include <metameric/core/array.hpp>
// #include <Eigen/Dense>

namespace met { 
  /* Define metameric's spectral range layout */
  constexpr static float  wavelength_min         = 360.f;  
  constexpr static float  wavelength_max         = 830.f;  
  constexpr static float  wavelength_range       = wavelength_max - wavelength_min;  
  constexpr static size_t wavelength_samples     = 32;
  constexpr static float  wavelength_sample_size = wavelength_range 
                                                 / static_cast<float>(wavelength_samples);  
 
  constexpr float wavelength_at_index(size_t i) {
    return (static_cast<float>(i) + .5f) * wavelength_sample_size + wavelength_min;
  }

  constexpr size_t index_at_wavelength(float f) {
    return static_cast<size_t>((f - wavelength_min) / wavelength_sample_size);
  }
  
  template <size_t Size, template <typename, size_t> typename C = std::array>
  class SpectrumArray : public Array<float, Size, C> {
    using wvl_type   = float;
    using value_type = float;
    using ref_type   = float &;
    using ptr_type   = float *;
    using cref_type  = const float &;
    using size_type  = size_t;
    
    using base_type  = Array<float, Size, C>;
    using mask_type  = MaskArray<Size, C>;

  public:
    /* constrs */

    using base_type::base_type;

    /* operators */

    met_array_decl_op_add(SpectrumArray);
    met_array_decl_op_sub(SpectrumArray);
    met_array_decl_op_mul(SpectrumArray);
    met_array_decl_op_div(SpectrumArray);
    met_array_decl_op_com(SpectrumArray);

    /* reductions */

    met_array_decl_red_val(SpectrumArray);
    met_array_decl_mod_val(SpectrumArray);

    /* spectrum-specific functions */

    constexpr ref_type operator()(wvl_type f) {
      return base_type::operator[](index_at_wavelength(f));
    }

    constexpr cref_type operator()(wvl_type f) const {
      return base_type::operator[](index_at_wavelength(f));
    }
  };

  /* template <size_t Size, template <typename, size_t> typename C = std::array>
  class CMFSArray : public Array<float, Size, C> {
    using wvl_type   = float;
    using value_type = float;
    using ref_type   = float &;
    using ptr_type   = float *;
    using cref_type  = const float &;
    using size_type  = size_t;
    
    using base_type  = Array<float, Size, C>;
    using mask_type  = MaskArray<Size, C>;
    
  public:
  }; */

  using Spectrum = SpectrumArray<wavelength_samples>;
  // using CMFS     = CMFSArray<wavelength_samples>;

  // namespace eig = Eigen;
  
  // using Spectrum = eig::Vector<float, wavelength_samples>;
  // using CMFS     = eig::Matrix<float, wavelength_samples, 3>;
} // met