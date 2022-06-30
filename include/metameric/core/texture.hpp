#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/io.hpp>

namespace met {
  struct CircumstanceInfo {
    // Color matching functions and illuminant under which observation is performed
    CMFS cmfs           = models::cmfs_cie_xyz;

    // Illuminant under which observation is performed
    Spec illuminant     = models::emitter_cie_d65;

    // Nr. of repeated scatterings and reflectances 
    uint n_reflectances = 1;
  };

  struct SpectralGamut {

  };
  
  struct RGBTexture;
  struct SpectralTexture;

  class RGBTexture {

  public:
    RGBTexture() = default;
    RGBTexture(const io::TextureData<float> data);
    ~RGBTexture();

    SpectralTexture to_spectral(const CircumstanceInfo &info, const SpectralGamut &gamut);
  };

  class SpectralTexture {

  public:
    SpectralTexture() = default;
    SpectralTexture(const io::TextureData<float> data);
    SpectralTexture(const RGBTexture &texture, 
                    const CircumstanceInfo &info, 
                    const SpectralGamut &gamut);
    ~SpectralTexture();

    RGBTexture to_rgb(const CircumstanceInfo& info);
    RGBTexture to_xyz(const CircumstanceInfo& info);
  };
} // namespace met