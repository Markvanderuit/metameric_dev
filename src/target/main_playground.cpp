// Metameric includes
#include <metameric/core/io.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>

// STL includes
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <numeric>
#include <span>
#include <string>
#include <vector>

using namespace met;
using uint = unsigned;

// Given a wavelength, obtain the relevant spectral bin's index
constexpr inline
size_t index_from_wavelength(float wvl) {
  int i = static_cast<int>((wvl - wavelength_min) * wavelength_ssinv);
  return std::min<int>(std::max<int>(i, 0), wavelength_samples - 1);
  // return std::min(static_cast<uint>((wvl - wavelength_min) * wavelength_ssinv), 
  //                 wavelength_samples - 1);
}

int main() {

  /* std::pair<
    std::vector<float>, 
    std::vector<float>
  > spectrum_to_data(const Spec &s) {

    for (uint i = 0; i < wavelength_samples; ++i) {
      float wvl = wavelength_min + i * wavelength_ssize;

    }

    // return { { }, { } };
  } */

  try {
    // Define color system
    CMFS cmfs = models::cmfs_cie_xyz; // (models::xyz_to_srgb_transform * models::cmfs_cie_xyz .transpose()).transpose().eval();
    Spec illm = models::emitter_cie_d65;
    ColrSystem system = { .cmfs = cmfs, .illuminant = illm };

    // Input signal
    Colr colr = { 0.75, 0.35, 0.25 };

    // Generate SD
    Basis basis = io::load_basis("resources/misc/basis.txt");
    Spec spec = generate_spectrum({
      .basis = basis,
      .systems = std::vector<CMFS> { system.finalize() },
      .signals = std::vector<Colr> { colr }
    });

    // Test roundtrip of signal
    Colr colr_rtrip = system.apply_color(spec);
    fmt::print("rtrp = {}\n", colr_rtrip);

    std::vector<float> wvls(wavelength_samples + 1), vals(wavelength_samples + 1);
    for (uint i = 0; i < wavelength_samples + 1; ++i) {
      float wvl = wavelength_min + (float(i) - 0.5) * wavelength_ssize;

      // uint bin_a = i, bin_b = i + 1;

      uint bin_a = index_from_wavelength(wvl),
           bin_b = index_from_wavelength(wvl + wavelength_ssize);
           
      wvls[i] = wavelength_min + i * wavelength_ssize;
      vals[i] = 0.5 * (spec[bin_a] + spec[bin_b]);
      // fmt::print("{} - {} : {}\n", i, bin_a, bin_b);
    }
    // std::exit(0);

    /* for (uint i = 0; i < wavelength_samples + 1; ++i) {
      float wvl = wavelength_min + (float(i) - 0.5) * wavelength_ssize;
      
      wvls[i] = wvl;

      uint bin_a = index_from_wavelength(wvl),
           bin_b = index_from_wavelength(wvl + wavelength_ssize);

      fmt::print("{} - {} : {}\n", i, bin_a, bin_b);

      if (bin_a == bin_b) {
        // vals[i] = ()
        // vals[i] = spec[bin_a];
        // vals[i] = 0.5 * (spec[bin_a] + spec[bin_b]);
        vals[i] = spec[bin_a];
      } else {
        float a = wavelength_at_index(bin_a),
              b = wavelength_at_index(bin_b),
              fa = spec[bin_a],
              fb = spec[bin_b];
        float w = (wvl - a) / (b - a);
        fmt::print("{}\n", w);
        vals[i] = w * fb + (1 - w) * fa;
      }
    } */
    // std::exit(0);

    // auto [wvls, vals] = io::spectrum_to_data(spec);
    Spec spec_rtrip = io::spectrum_from_data(wvls, vals);
    colr_rtrip = system.apply_color(spec_rtrip);
    fmt::print("rtrp 2 = {}\n\n", colr_rtrip);

    fmt::print("wvls = {}\n\n", wvls);
    fmt::print("vals = {}\n\n", vals);

    fmt::print("spec = {}\n\n", spec);
    fmt::print("spec_rtrip = {}\n\n", spec_rtrip);
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}