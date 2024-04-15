#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <array>
#include <complex>
#include <numbers>

namespace met {
  // Mostly copied from https://momentsingraphics.de/Siggraph2019.html -> MomentBasedSpectra.hlsl, 
  // following the paper "Using Moments to Represent Bounded Signals for Spectral Rendering", 
  // Peters et al., 2019.

  using MomentsCN = eig::Array<eig::scomplex, moment_coeffs, 1>;

  // Helpers to convert wavelengths in this program's smaller wavelength range
  // to the correct phase warp in the full CIE range.
  constexpr float warp_wavelength_min = 360.f;
  constexpr float warp_wavelength_max = 830.f;
  constexpr float warp_offs = (static_cast<float>(wavelength_min) - warp_wavelength_min) 
                            / (warp_wavelength_max - warp_wavelength_min);
  constexpr float warp_mult = (static_cast<float>(wavelength_max - wavelength_min)) 
                            / (warp_wavelength_max - warp_wavelength_min);

  // Fitted phase warp data for the full CIE range.
  const eig::Array<float, 95, 1> warp_data = { 
    -3.141592654, -3.141592654, -3.141592654, -3.141592654, -3.141591857, -3.141590597, -3.141590237,
    -3.141432053, -3.140119041, -3.137863071, -3.133438967, -3.123406739, -3.106095749, -3.073470612,
    -3.024748900, -2.963566246, -2.894461907, -2.819659701, -2.741784136, -2.660533432, -2.576526605,
    -2.490368187, -2.407962868, -2.334138406, -2.269339880, -2.213127747, -2.162806279, -2.114787412,
    -2.065873394, -2.012511127, -1.952877310, -1.886377224, -1.813129945, -1.735366957, -1.655108108,
    -1.573400329, -1.490781436, -1.407519056, -1.323814008, -1.239721795, -1.155352390, -1.071041833,
    -0.986956525, -0.903007113, -0.819061538, -0.735505101, -0.653346027, -0.573896987, -0.498725202,
    -0.428534515, -0.363884284, -0.304967687, -0.251925536, -0.205301867, -0.165356255, -0.131442191,
    -0.102998719, -0.079687644, -0.061092401, -0.046554594, -0.035419229, -0.027113640, -0.021085743,
    -0.016716885, -0.013468661, -0.011125245, -0.009497032, -0.008356318, -0.007571826, -0.006902676,
    -0.006366945, -0.005918355, -0.005533442, -0.005193920, -0.004886397, -0.004601975, -0.004334090,
    -0.004077698, -0.003829183, -0.003585923, -0.003346286, -0.003109231, -0.002873996, -0.002640047,
    -0.002406990, -0.002174598, -0.001942639, -0.001711031, -0.001479624, -0.001248405, -0.001017282,
    -0.000786134, -0.000557770, -0.000332262,  0.000000000 };

  namespace detail {
    MomentsCN trigonometric_to_exponential_moments(const Moments &tm) {
      MomentsCN em = MomentsCN::Zero();
      
      float zeroeth_phase = tm[0] * std::numbers::pi_v<float> - 0.5f * std::numbers::pi_v<float>;
      em[0] = .0795774715f * eig::scomplex(std::cos(zeroeth_phase), std::sin(zeroeth_phase));
      
      for (uint i = 1; i < moment_coeffs; ++i) {
        for (uint j = 0; j < i; ++j) {
          em[i] += tm[i - j] 
                 * em[j] 
                 * eig::scomplex(0.f, 
                    std::numbers::pi_v<float> * 2.f *
                    static_cast<float>(i - j) / static_cast<float>(i));
        } // for (uint j)
      } // for (uint i)

      em[0] = 2.0f * em[0];

      return em;
    }

    MomentsCN levinsons_algorithm(const MomentsCN &fc) { // first column
      MomentsCN rm = MomentsCN::Zero();
      rm[0] = eig::scomplex(1.f / fc[0].real());

      for (uint i = 1; i < moment_coeffs; ++i) {
        eig::scomplex dp = rm[0].real() * fc[i];
        for (uint j = 1; j < i; ++j)
          dp += rm[j] * fc[i - j];
        float factor = 1.f / (1.f - std::norm(dp));
        
        MomentsCN flipped_solution;
        for (uint j = 1; j < i; ++j)
          flipped_solution[j] = std::conj(rm[i - j]);             // 1-6, 7 - 1:6 = 6-1
        flipped_solution[i] = eig::scomplex(rm[0].real());        // 7

        rm[0] = eig::scomplex(factor * rm[0].real());             // 0
        for (uint j = 1; j < i; ++j)
          rm[j] = factor * (rm[j] - flipped_solution[j] * dp);    // 1-6
        rm[i] = factor * (-flipped_solution[i].real() * dp);      // 7
      } // for (uint i)

      return rm;
    }

    eig::scomplex fast_herglotz_trf(const eig::scomplex &circle_point, const MomentsCN &em, const MomentsCN &pm) {
      eig::scomplex conj_circle_point = std::conj(circle_point);
      
      MomentsCN polynomial;
      polynomial[0] = pm[0].real();
      for (uint j = 1; j < moment_coeffs; ++j)
        polynomial[j] = pm[j] + polynomial[j - 1] * conj_circle_point;

      eig::scomplex dp = 0.0;
      for (uint j = 1; j < moment_coeffs; ++j)
        dp += polynomial[moment_coeffs - j - 1] * em[j];

      return em[0] + 2.f * dp / polynomial[moment_coeffs - 1];
    }

    std::pair<MomentsCN, MomentsCN> prepare_reflectance(const Moments &bm) {
      auto em = trigonometric_to_exponential_moments(bm);
      auto pm = levinsons_algorithm(em);
      for (uint i = 0; i < moment_coeffs; ++i)
        pm[i] = 2.f * std::numbers::pi_v<float> * pm[i];
      return { em, pm };
    }

    MomentsCN prepare_reflectance_lagrange(const Moments &bm) {
      using namespace std::complex_literals;

      auto em = trigonometric_to_exponential_moments(bm);
      auto pm = levinsons_algorithm(em);
      for (uint i = 0; i < moment_coeffs; ++i)
        pm[i] = 2.f * std::numbers::pi_v<float> * pm[i];
      
      MomentsCN auto_correlation = MomentsCN::Zero();
      for (uint k = 0; k < moment_coeffs; ++k)
        for (uint j = 0; j < moment_coeffs - k; ++j)
          auto_correlation[k] += pm[j] * std::conj(pm[j + k]);
          // auto_correlation[k] += std::conj(pm[j + k] * pm[j]);
      
      em[0] *= 0.5f;

      MomentsCN lagrange = MomentsCN::Zero();
      for (uint k = 0; k < moment_coeffs; ++k)
        for (uint j = 0; j < moment_coeffs - k; ++j)
          lagrange[k] += em[j] * auto_correlation[j + k];
      lagrange /= (std::numbers::pi_v<float> * 1.if * pm[0]);
          
      // lagrange *= eig::scomplex(-0.f, -1.f);
      // lagrange[0] = eig::scomplex(lagrange[0].real(), 0.f);
      
      return lagrange;
    }
    
    float evaluate_reflectance(float phase, const MomentsCN &em, const MomentsCN &pm){
      eig::scomplex circle_point = { std::cos(phase), std::sin(phase) };
      auto trf = fast_herglotz_trf(circle_point, em, pm);
      return std::atan2(trf.imag(), trf.real()) * std::numbers::inv_pi_v<float> + 0.5f;
    }

    float evaluate_reflectance_lagrange(float phase, const MomentsCN &lagrange) {
      using namespace std::complex_literals;
      
      float fourier_series = 0.f;
      for (uint k = 1; k < moment_coeffs; ++k)
        fourier_series += (lagrange[k] * std::exp(-1.if * static_cast<float>(k) * phase)).real();
      fourier_series = 2.f * fourier_series + lagrange[0].real();

      return std::atan(fourier_series) * std::numbers::inv_pi_v<float> + .5f;
    }
      
    // If normalize_wvl is set, maps [wvl_min, wvl_max] to [0, 1] first. Otherwise
    // treats wvl as a value in [0, 1]. Output is then mapped to [-pi, 0].
    float wavelength_to_phase(float wvl, bool normalize_wvl = false) {
      if (normalize_wvl)
        wvl = (wvl - static_cast<float>(wavelength_min)) / static_cast<float>(wavelength_range);
      return std::numbers::pi_v<float> * wvl - std::numbers::pi_v<float>;
    }
  } // namespace detail


  Moments spectrum_to_moments(const Spec &s) {
    met_trace();

    using namespace std::complex_literals;
    using BSpec = eig::Array<double, wavelength_samples + 2, 1>;
    using DMomn = eig::Array<eig::dcomplex, moment_coeffs, 1>;

    eig::Array<float, wavelength_samples, 1> phase_samples;
    rng::copy(vws::iota(0u, wavelength_samples) 
      | vws::transform([](auto i) { return (static_cast<float>(i) + .5f) / static_cast<float>(wavelength_samples); })
      | vws::transform([](auto f) { return (f * warp_mult) + warp_offs; }),
      phase_samples.begin());
    
    // Expand out of range values for phase and signal 
    auto phase  = (BSpec() << -std::numbers::pi, 
                              eig::interp(phase_samples, warp_data).cast<double>(), 
                              0.0).finished();
    auto signal = (BSpec() << s.cast<double>().head<1>(), 
                              s.cast<double>(), 
                              s.cast<double>().tail<1>()).finished();

    // Initialize real/complex parts of value as all zeroes
    DMomn moments = DMomn::Zero();

    // Handle integration
    for (uint i = 0; i < BSpec::RowsAtCompileTime - 1; ++i) {
      guard_continue(phase[i] < phase[i + 1]);

      auto gradient = (signal[i + 1] - signal[i]) / (phase[i + 1]  - phase[i]);      
      auto y_inscpt = signal[i] - gradient * phase[i];

      for (uint j = 1; j < moment_coeffs; ++j) {
        auto rcp_j2 = 1.0 / static_cast<double>(j * j);
        auto flt_j  = static_cast<double>(j);

        auto common_summands = gradient * rcp_j2 
                             + y_inscpt * (1.0i / flt_j);
        moments[j] += (common_summands + gradient * 1.0i * flt_j * phase[i + 1] * rcp_j2)
                    * std::exp(-1.0i * flt_j * phase[i + 1]);
        moments[j] -= (common_summands + gradient * 1.0i * flt_j * phase[i] * rcp_j2)
                    * std::exp(-1.0i * flt_j * phase[i]);
      } // for (uint j)

      moments[0] += 0.5 * gradient * (phase[i + 1] * phase[i + 1]) + y_inscpt * phase[i + 1];
      moments[0] -= 0.5 * gradient * (phase[i] * phase[i]) + y_inscpt * phase[i];
    } // for (uint i)

    moments *= 0.5 * std::numbers::inv_pi;
    return (2.0 * moments.real()).cast<float>().eval();
  }

  Spec moments_to_spectrum(const Moments &bm) {
    met_trace();
    using namespace std::placeholders;
    auto [em, pm] = detail::prepare_reflectance(bm);
    Spec s;
    rng::transform(generate_warped_phase(), s.begin(), std::bind(detail::evaluate_reflectance, _1, em, pm));
    return s;
  }

  Spec moments_to_spectrum_lagrange(const Moments &m) {
    met_trace();
    using namespace std::placeholders;
    auto lagrange = detail::prepare_reflectance_lagrange(m);
    Spec s;
    rng::transform(generate_warped_phase(), s.begin(), std::bind(detail::evaluate_reflectance_lagrange, _1, lagrange));
    return s;
  }
  
  float moments_to_reflectance(float wvl, const Moments &m) {
    met_trace();
    auto phase    = detail::wavelength_to_phase(wvl);
    auto [em, pm] = detail::prepare_reflectance(m);
    return detail::evaluate_reflectance(phase, em, pm);
  }

  eig::Array4f moments_to_reflectance(const eig::Array4f &wvls, const Moments &m) {
    met_trace();

    using namespace std::placeholders;

    eig::Array4f phase;
    rng::transform(wvls, phase.begin(), std::bind(detail::wavelength_to_phase, _1, false));

    auto [em, pm] = detail::prepare_reflectance(m);

    eig::Array4f s;
    rng::transform(phase, s.begin(), std::bind(detail::evaluate_reflectance, _1, em, pm));

    return s;
  }

  Spec generate_uniform_phase() {
    Spec x;
    rng::copy(vws::iota(0u, wavelength_samples) 
      | vws::transform([](uint i) { return (static_cast<float>(i) + .5f) / static_cast<float>(wavelength_samples); })
      | vws::transform(std::bind(detail::wavelength_to_phase, std::placeholders::_1, false)),
      x.begin());
    return x;
  }

  Spec generate_warped_phase() {
    Spec x;
    eig::Array<float, wavelength_samples, 1> phase_samples;
    rng::copy(vws::iota(0u, wavelength_samples) 
      | vws::transform([](auto i) { return (static_cast<float>(i) + .5f) / static_cast<float>(wavelength_samples); })
      | vws::transform([](auto f) { return (f * warp_mult) + warp_offs; }),
      phase_samples.begin());
    return eig::interp(phase_samples, warp_data);
  }
} // namespace met