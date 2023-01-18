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
    CMFS cmfs = models::cmfs_cie_xyz;
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

    constexpr auto wvl_at_i = [](int i) {
      return wavelength_min + (float(i) + 0.5) * wavelength_ssize;
    };
    constexpr auto i_at_wvl = [](float wvl) {
      return std::clamp(uint((wvl - wavelength_min) * wavelength_ssinv - 0.5), 0u, wavelength_samples - 1);
    };

    std::vector<float> wvls(wavelength_samples + 1), vals(wavelength_samples + 1);
    for (uint i = 0; i < wavelength_samples + 1; ++i) {
      float wvl = wavelength_min + float(i) * wavelength_ssize;

      uint bin_a = i_at_wvl(wvl),
           bin_b = i_at_wvl(wvl + wavelength_ssize);
      float wvl_a = wvl_at_i(bin_a),
            wvl_b = wvl_at_i(bin_b);
           
      wvls[i] = wvl;
      if (bin_a == bin_b) {
        vals[i] = spec[bin_a];
      } else {
        float alpha = (wvl - wvl_a) / (wvl_b - wvl_a);
        vals[i] = (1.f - alpha) * spec[bin_a] + alpha * spec[bin_b];
      }

      fmt::print("input = {}, bins = ({}, {}), wvls = ({}, {})\n", 
        wvl, bin_a, bin_b, wvl_a, wvl_b);
    }

    // // auto [wvls, vals] = io::spectrum_to_data(spec);
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