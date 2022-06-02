#pragma once

#include <metameric/core/array.hpp>

namespace met {
  /* Define program's wavelength layout */
  constexpr static size_t wavelength_samples = 31;
  constexpr static float wavelength_min = 400.f;  
  constexpr static float wavelength_max = 710.f;  
  constexpr static float wavelength_range = wavelength_max - wavelength_min;  
  constexpr static float wavelength_sample_size = wavelength_range / static_cast<float>(wavelength_samples);  
 
  constexpr float wavelength_at_index(size_t i) {
    return wavelength_min + (static_cast<float>(i) + .5f) * wavelength_sample_size;
  }

  constexpr size_t index_at_wavelength(float f) {
    return static_cast<size_t>((f - wavelength_min) / wavelength_sample_size);
  }
  
  template <size_t Size, template <typename, size_t> typename C = std::array>
  class _SpectrumArray : public _Array<float, Size, C> {
    using wvl_type   = float;
    using value_type = float;
    using ref_type   = float &;
    using ptr_type   = float *;
    using cref_type  = const float &;
    using size_type  = size_t;
    
    using base_type  = _Array<float, Size, C>;
    using mask_type  = _MaskArray<Size, C>;

  public:
    /* constrs */

    using base_type::base_type;

    /* operators */

    met_array_decl_op_add(_SpectrumArray);
    met_array_decl_op_sub(_SpectrumArray);
    met_array_decl_op_mul(_SpectrumArray);
    met_array_decl_op_div(_SpectrumArray);
    met_array_decl_op_com(_SpectrumArray);

    /* reductions */

    met_array_decl_red_val(_SpectrumArray);
    met_array_decl_mod_val(_SpectrumArray);

    /* spectrum-specific functions */

    constexpr ref_type operator()(wvl_type f) {
      return base_type::operator[](index_at_wavelength(f));
    }

    constexpr cref_type operator()(wvl_type f) const {
      return base_type::operator[](index_at_wavelength(f));
    }
  };

  using Spectrum = _SpectrumArray<wavelength_samples>;

  class StaticSpectrum : public StaticArray<float, wavelength_samples> {
    using Base = StaticArray<float, wavelength_samples>;

  public:
    /* Base class functions */

    using Base::Base;
    using Base::size;
    using Base::operator[];

    /* operators */

    met_array_decl_operators(Base, StaticSpectrum, float);
    met_array_decl_reductions(StaticSpectrum, float);
    met_array_decl_comparators(StaticSpectrum, float);
  };

  class DynamicSpectrum : public DynamicArray<float> {
    using Base = DynamicArray<float>;

  public:
    /* Base class functions */

    using Base::Base;
    using Base::size;
    using Base::operator[];

    /* operators */

    met_array_decl_operators(Base, DynamicSpectrum, float);
    met_array_decl_reductions(DynamicSpectrum, float);
    met_array_decl_comparators(DynamicSpectrum, float);
  };
} // met