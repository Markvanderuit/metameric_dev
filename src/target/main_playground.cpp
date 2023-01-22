// Metameric includes
#include <metameric/core/io.hpp>
#include <metameric/core/data.hpp>
#include <metameric/core/json.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/metamer.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <nlohmann/json.hpp>

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
  try {
    // Load bases
    BasisTreeNode tree = io::load_json("resources/misc/tree.json").get<BasisTreeNode>();

    // Define color system
    CMFS cmfs = models::cmfs_cie_xyz;
    Spec illm = models::emitter_cie_d65;
    ColrSystem system = { .cmfs = cmfs, .illuminant = illm };

    // Color signals
    Colr colr_0 = { 0.81634661,  0.19187019,  0.02059487 }; 
    Colr colr_1 = { 0.03438915,  0.04543598,  0.26456009 }; 

    // Uplift to reflectances
    Spec spec_0 = generate_spectrum({
      .basis = tree.basis,
      .basis_avg = tree.basis_mean,
      .systems = std::vector<CMFS> { system.finalize() },
      .signals = std::vector<Colr> { colr_0 }
    });
    Spec spec_1 = generate_spectrum({
      .basis = tree.basis,
      .basis_avg = tree.basis_mean,
      .systems = std::vector<CMFS> { system.finalize() },
      .signals = std::vector<Colr> { colr_1 }
    });

    colr_0 = lrgb_to_xyz(colr_0);
    colr_1 = lrgb_to_xyz(colr_1);

    // Define used storage
    std::vector<float> alpha(16);
    std::vector<Colr> colr_alpha(alpha.size());
    std::vector<Colr> spec_alpha(alpha.size());
    
    // Define alpha parameters
    float step = 1.f / static_cast<float>(alpha.size() - 1);
    std::generate(range_iter(alpha), [step, a = 0.f] () mutable {
      auto a_ = a;
      a += step;
      return a_;
    });

    // Perform interpolations
    std::ranges::transform(alpha, colr_alpha.begin(), [&](float a) {
      return lrgb_to_srgb(xyz_to_lrgb((1.f - a) * colr_0 + a * colr_1));
    });
    std::ranges::transform(alpha, spec_alpha.begin(), [&](float a) {
      Spec spec_a = (1.f - a) * spec_0 + a * spec_1;
      return lrgb_to_srgb(system.apply_color(spec_a));
    });

    fmt::print("alpha = {}\n", alpha);
    fmt::print("colr_alpha = {}\n", colr_alpha);
    fmt::print("spec_alpha = {}\n", spec_alpha);

    // auto [wvls, vals_0] = io::spectrum_to_data(spec_0);
    // auto [_, vals_1] = io::spectrum_to_data(spec_1);
    // fmt::print("wvls = np.array([{}])\n", wvls);
    // fmt::print("spec_0 = np.array([{}])\n", vals_0);
    // fmt::print("spec_1 = np.array([{}])\n", vals_1);
  } catch (const std::exception &e) {
    fmt::print(stderr, "{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}