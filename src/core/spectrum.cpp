#include <metameric/core/io.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/serialization.hpp>
#include <metameric/core/ranges.hpp>
#include <algorithm>
#include <execution>
#include <unordered_set>

namespace met {
  namespace models {
    #include <metameric/core/detail/spectrum_models.ext>
    
    // Linear color space transformations
    eig::Matrix3f xyz_to_srgb_transform {{ 3.240479f, -1.537150f,-0.498535f },
                                         {-0.969256f,  1.875991f, 0.041556f },
                                         { 0.055648f, -0.204043f, 1.057311f }};
    eig::Matrix3f srgb_to_xyz_transform {{ 0.412453f, 0.357580f, 0.180423f },
                                         { 0.212671f, 0.715160f, 0.072169f },
                                         { 0.019334f, 0.119193f, 0.950227f }};
    eig::Matrix3f xyz_to_rec709_transform {{ 3.2409699419f,-1.5373831776f,-0.4986107603 },
                                           {-0.9692436363f, 1.8759675015f, 0.0415550574 },
                                           { 0.0556300797f,-0.2039769589f, 1.0569715142 }};
    eig::Matrix3f xyz_to_rec2020_transform {{ 1.7166511880f,-0.3556707838f,-0.2533662814f },
                                            {-0.6666843518f, 1.6164812366f, 0.0157685458f },
                                            { 0.0176398574f,-0.0427706133f, 0.9421031212f }};
    eig::Matrix3f xyz_to_ap1_transform {{ 1.6410233797f,-0.3248032942f,-0.2364246952f },
                                        {-0.6636628587f, 1.6153315917f, 0.0167563477f },
                                        { 0.0117218943f,-0.0082844420f, 0.9883948585f }};
    eig::Matrix3f rec2020_to_xyz_transform = xyz_to_rec2020_transform.inverse().eval();
    eig::Matrix3f rec709_to_xyz_transform = xyz_to_rec709_transform.inverse().eval();
    eig::Matrix3f ap1_to_xyz_transform = xyz_to_ap1_transform.inverse().eval();
    
    // Color matching functions
    CMFS cmfs_cie_xyz = io::cmfs_from_data(cie_wavelength_values, cie_xyz_values_x, cie_xyz_values_y, cie_xyz_values_z);

    // Illuminant spectra
    Spec emitter_cie_e       = 1.f;
    Spec emitter_cie_d65     = io::spectrum_from_data(cie_wavelength_values, cie_d65_values);
    Spec emitter_cie_fl2     = io::spectrum_from_data(cie_wavelength_values, cie_fl2_values);
    Spec emitter_cie_fl11    = io::spectrum_from_data(cie_wavelength_values, cie_fl11_values);
    Spec emitter_cie_ledb1   = io::spectrum_from_data(cie_wavelength_values, cie_ledb1_values);
    Spec emitter_cie_ledrgb1 = io::spectrum_from_data(cie_wavelength_values, cie_ledrgb1_values);
  } // namespace models

  Colr srgb_to_lrgb(Colr c) { rng::transform(c, c.begin(), srgb_to_lrgb_f); return c; }
  Colr lrgb_to_srgb(Colr c) { rng::transform(c, c.begin(), lrgb_to_srgb_f); return c; }

  void accumulate_spectrum(Spec &s, const eig::Array4f &wvls, const eig::Array4f &values) {
    for (auto [wvl, value] : vws::zip(wvls, values))
      accumulate_spectrum(s, wvl, value);
  }

  Spec accumulate_spectrum(const eig::Array4f &wvls, const eig::Array4f &values) {
    Spec s = 0.f;
    for (auto [wvl, value] : vws::zip(wvls, values))
      accumulate_spectrum(s, wvl, value);
    return s;
  }

  Colr integrate_cmfs(const CMFS &cmfs, eig::Array4f wvls, eig::Array4f values) {
    Colr c = 0.f;
    for (auto [wvl, value] : vws::zip(wvls, values))
      c += sample_cmfs(cmfs, wvl) * value;
    return c;
  }

  eig::Array4f sample_spectrum(const eig::Array4f &wvls, const Spec &s) {
    eig::Array4f v = 0.f;
    for (uint i = 0; i < 4; ++i)
      v[i] = sample_spectrum(wvls[i], s);
    return v;
  }

  CMFS ColrSystem::finalize(bool as_rgb) const {
    met_trace();
    CMFS csys = (cmfs.array().colwise() * illuminant)
              / (cmfs.array().col(1)    * illuminant).sum();
    if (as_rgb)
      csys = (models::xyz_to_srgb_transform * csys.matrix().transpose()).transpose().eval();
    return csys;
  }
  
  Colr ColrSystem::apply(const Spec &s, bool as_rgb) const { 
    met_trace();
    return finalize(as_rgb).transpose() * s.matrix().eval();
  }
  
  std::vector<Colr> ColrSystem::apply(std::span<const Spec> sd, bool as_rgb) const { 
    met_trace();
    
    // Gather color system data into matrix
    auto csys = finalize(as_rgb);

    // Transform to non-unique colors
    std::vector<Colr> out(sd.size());
    std::transform(std::execution::par_unseq, range_iter(sd), out.begin(),
      [&csys](const auto &s) -> Colr { return (csys.transpose() * s.matrix()).eval(); });

    // Collapse return value to unique colors
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > out_unique(range_iter(out));
    return std::vector<Colr>(range_iter(out_unique));
  }

  bool ColrSystem::operator==(const ColrSystem &o) const { 
    return cmfs.isApprox(o.cmfs) && illuminant.isApprox(o.illuminant);
  }

  void ColrSystem::to_stream(std::ostream &str) const {
    met_trace();
    io::to_stream(cmfs,       str);
    io::to_stream(illuminant, str);
  }

  void ColrSystem::fr_stream(std::istream &str) {
    met_trace();
    io::fr_stream(cmfs,       str);
    io::fr_stream(illuminant, str);
  }
  
  std::vector<CMFS> IndirectColrSystem::finalize(bool as_rgb) const {
    met_trace();
    CMFS csys = cmfs.array() / cmfs.col(1).sum();
    if (as_rgb)
      csys = (models::xyz_to_srgb_transform * csys.matrix().transpose()).transpose();
    return powers | vws::transform([&csys](const Spec &pwr) { 
      return (csys.array().colwise() * pwr).eval(); }) | rng::to<std::vector<CMFS>>();
  }

  Colr IndirectColrSystem::apply(const Spec &s, bool as_rgb) const {
    met_trace();
    Colr c = 0.f;
    auto csys = finalize(as_rgb);
    for (auto [i, csys] : enumerate_view(csys))
      c += (csys.transpose() * s.pow(static_cast<float>(i)).matrix()).array().eval();
    return c;
  }

  std::vector<Colr> IndirectColrSystem::apply(std::span<const Spec> sd, bool as_rgb) const {
    met_trace();

    // Gather color system data into matrices
    auto csys = finalize(as_rgb);
    
    // Transform to non-unique colors
    std::vector<Colr> out(sd.size());
    std::transform(std::execution::par_unseq, range_iter(sd), out.begin(),
    [&csys](const auto &s) -> Colr { 
      Colr c = 0.f;
      for (auto [i, csys] : enumerate_view(csys))
        c += (csys.transpose() * s.pow(static_cast<float>(i)).matrix()).array().eval();
      return c;
    });

    // Collapse return value to unique colors
    std::unordered_set<
      Colr, 
      decltype(Eigen::detail::matrix_hash<float>), 
      decltype(Eigen::detail::matrix_equal)
    > out_unique(range_iter(out));
    return std::vector<Colr>(range_iter(out_unique));
  }

  bool IndirectColrSystem::operator==(const IndirectColrSystem &o) const {
    return cmfs.isApprox(o.cmfs) && rng::equal(powers, o.powers, eig::safe_approx_compare<Spec>);
  }

  void IndirectColrSystem::to_stream(std::ostream &str) const {
    met_trace();
    io::to_stream(cmfs,   str);
    io::to_stream(powers, str);
  }

  void IndirectColrSystem::fr_stream(std::istream &str) {
    met_trace();
    io::fr_stream(cmfs,   str);
    io::fr_stream(powers, str);
  }
  
  bool Basis::operator==(const Basis &o) const {
    return mean.isApprox(o.mean) && func.isApprox(o.func);
  }

  void Basis::to_stream(std::ostream &str) const {
    met_trace();
    io::to_stream(mean, str);
    io::to_stream(func, str);
  }

  void Basis::fr_stream(std::istream &str) {
    met_trace();
    io::fr_stream(mean, str);
    io::fr_stream(func, str);
  }
} // namespace met
