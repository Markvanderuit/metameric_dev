#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/spectrum.hpp>
#include <small_gl/buffer.hpp>
#include <vector>

namespace met {
  /**
   * Data object to describe circumstances in which which a spectral
   * to color mapping can be performed.
   */
  struct SpectralMapping {
    // Color matching functions and illuminant under which observations are performed
    CMFS cmfs       = models::cmfs_srgb;
    Spec illuminant = models::emitter_cie_d65;

    // Nr. of repeated scatterings of a reflectance
    uint n_scatterings = 0;
  };
  
  /**
   * Data object to describe a region in linear sRGB space formed by
   * the projection of four spectral distributions.
   */
  struct SpectralGamut {
    std::vector<Color> vertices;
    std::vector<Spec>  spectra;
  };
} // namespace met