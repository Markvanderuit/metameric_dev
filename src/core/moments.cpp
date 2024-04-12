#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <complex>
#include <numbers>

namespace met {
  // Mostly copied from https://momentsingraphics.de/Siggraph2019.html -> MomentBasedSpectra.hlsl, 
  // following the paper "Using Moments to Represent Bounded Signals for Spectral Rendering", 
  // Peters et al., 2019.

  using MomentsCN = eig::Array<eig::scomplex, moment_coeffs, 1>;

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

  // TODO; add warp
  Moments spectrum_to_moments(const Spec &input_signal) {
    met_trace();

    using namespace std::complex_literals;
    using namespace std::placeholders;

    // Expand out of range values for phase and signal 
    using BSpec = eig::Array<float, wavelength_samples + 2, 1>;
    auto phase  = (BSpec() << -std::numbers::pi_v<float>, get_space_warp(), 0.0).finished();
    auto signal = (BSpec() << input_signal.head<1>(), input_signal, input_signal.tail<1>()).finished();

    // Initialize real/complex parts of value as all zeroes
    MomentsCN moments = MomentsCN::Zero();

    // Handle integration
    for (uint i = 0; i < BSpec::RowsAtCompileTime - 1; ++i) {
      guard_continue(phase[i] < phase[i + 1]);

      auto gradient = (signal[i + 1] - signal[i]) / (phase[i + 1]  - phase[i]);      
      auto y_inscpt = signal[i] - gradient * phase[i];

      for (uint j = 1; j < moment_coeffs; ++j) {
        auto rcp_j2 = 1.f / static_cast<float>(j * j);
        auto flt_j  = static_cast<float>(j);

        auto common_summands = gradient * rcp_j2 
                             + y_inscpt * (1.0if / flt_j);
        moments[j] += (common_summands + gradient * 1.0if * flt_j * phase[i + 1] * rcp_j2)
                    * std::exp(-1.0if * flt_j * phase[i + 1]);
        moments[j] -= (common_summands + gradient * 1.0if * flt_j * phase[i] * rcp_j2)
                    * std::exp(-1.0if * flt_j * phase[i]);
      } // for (uint j)

      moments[0] += .5f * gradient * (phase[i + 1] * phase[i + 1]) + y_inscpt * phase[i + 1];
      moments[0] -= .5f * gradient * (phase[i] * phase[i]) + y_inscpt * phase[i];
    } // for (uint i)

    moments *= .5f * std::numbers::inv_pi_v<float>;
    return (2.f * moments.real()).eval();
  }

  Spec moments_to_spectrum(const Moments &bm) {
    met_trace();
    using namespace std::placeholders;
    auto [em, pm] = detail::prepare_reflectance(bm);
    Spec s;
    rng::transform(get_space_warp(), s.begin(), std::bind(detail::evaluate_reflectance, _1, em, pm));
    return s;
  }

  Spec moments_to_spectrum_lagrange(const Moments &m) {
    met_trace();
    using namespace std::placeholders;
    auto lagrange = detail::prepare_reflectance_lagrange(m);
    Spec s;
    rng::transform(get_space_warp(), s.begin(), std::bind(detail::evaluate_reflectance_lagrange, _1, lagrange));
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

  Spec get_space_warp() {
    static_assert(wavelength_min == 380.f && wavelength_max == 780.f && wavelength_samples == 64,
                  "This space warp for wavelength-to-phase needs to be updated for the current spectral layout");
    return { -3.1415926540f, -3.1415926540f, -3.1415921185f, -3.1415905464f, -3.1414938436f,
             -3.1399427933f, -3.1354436391f, -3.1231362548f, -3.0902929483f, -3.0270327303f,
             -2.9344128530f, -2.8278411923f, -2.7125846642f, -2.5909652784f, -2.4659041079f,
             -2.3514410143f, -2.2561651613f, -2.1777454648f, -2.1063803152f, -2.0316881917f,
             -1.9456038631f, -1.8440311408f, -1.7316048235f, -1.6129775345f, -1.4920723562f,
             -1.3695902061f, -1.2462914991f, -1.1224185787f, -0.9987810214f, -0.8754624712f,
             -0.7524775023f, -0.6322423757f, -0.5186927074f, -0.4154024368f, -0.3242996954f,
             -0.2453690825f, -0.1809600097f, -0.1292200447f, -0.0902504749f, -0.0608652478f,
             -0.0411609016f, -0.0275029645f, -0.0192426310f, -0.0138239355f, -0.0105401060f,
             -0.0085523782f, -0.0073731721f, -0.0064925070f, -0.0058281410f, -0.0052947156f,
             -0.0048375120f, -0.0044303612f, -0.0040505167f, -0.0036885483f, -0.0033351740f,
             -0.0029879380f, -0.0026437025f, -0.0023016874f, -0.0019607608f, -0.0016206376f,
             -0.0012809202f, -0.0009414366f, -0.0006041564f, -0.0002440049f };
  }
} // namespace met