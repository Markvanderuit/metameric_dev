#pragma once

#include <metameric/core/array.hpp>

namespace met {
  /* Define program's wavelength layout */
  constexpr static size_t wavelength_samples = 31;
  constexpr static float wavelength_min = 400.f;  
  constexpr static float wavelength_max = 710.f;  
  constexpr static float wavelength_range = wavelength_max - wavelength_min;  

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