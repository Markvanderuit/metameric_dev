#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <complex>

namespace met {
  using CMom  = eig::Array<eig::dcomplex, moment_samples + 1, 1>;
  using CSpec = eig::Array<eig::dcomplex, wavelength_samples, 1>;
  
  namespace detail {
    CMom bounded_to_exponential_moments(const Moments &bm) {
      met_trace();

      using namespace std::complex_literals;

      CMom em = CMom::Zero();
      em[0] = .25 * std::numbers::inv_pi
            * std::exp(std::numbers::pi * 1.0i * (static_cast<double>(bm[0]) - .5));
      
      for (uint i = 1; i < moment_samples + 1; ++i) {
        for (uint j = 0; j < i; ++j) {
          em[i] += static_cast<double>(i - j)
                 * em[j] 
                 * static_cast<double>(bm[i - j]);
        } // for (uint j)

        em[i] *= 2.i 
               * std::numbers::pi
               / static_cast<double>(i);
      } // for (uint i)

      return em;
    }

    CMom levisons_algorithm(const CMom &fc) { // first column
      met_trace();

      CMom rm = CMom::Zero();
      rm[0] = 1.0 / fc[0];

      for (uint i = 1; i < moment_samples + 1; ++i) {
        auto dp   = rm.head(i).matrix().conjugate().transpose()
                      .dot(fc(eig::seq(i, 1, eig::fix<-1>)).matrix());
        rm.head(i + 1) = (rm.head(i + 1) 
                          - dp * rm.head(i + 1).reverse().conjugate())
                       / (1.0 - std::abs(dp) * std::abs(dp));
      } // for (uint i)

      return rm;
    }

    CSpec herglotz_transform(const CSpec &point_in_disk, const CMom &em, const CMom &polynomial) {
      std::array<CSpec, moment_samples + 1> coefficient_list; // last coeff missing
      
      coefficient_list[moment_samples] = polynomial[0];
      for (int i = moment_samples - 1; i >= 0; --i) {
        coefficient_list[i] = polynomial[moment_samples - i]
                            + coefficient_list[i + 1] / point_in_disk;
      } // for (int i)
      
      CSpec trf = CSpec::Zero();
      for (int i = 1; i < moment_samples + 1; ++i) {
        trf += coefficient_list[i] * em[i];
      } // for (int i)

      trf *= 2.0 / coefficient_list[0];
      trf += em[0];
      
      return trf;
    }
  } // namespace detail

  // TODO; add warp
  Moments spectrum_to_moments(const Spec &input_signal) {
    met_trace();

    using namespace std::complex_literals;
    using namespace std::placeholders;
    using BSpec = eig::Array<float, wavelength_samples + 2, 1>;

    // Get vector of wavelength phases // TODO extract from computation
    Spec input_phase;
    rng::copy(vws::iota(0u, wavelength_samples) 
      // | vws::transform(wavelength_at_index)
      | vws::transform([](uint i) {
        return wavelength_min + wavelength_ssize * static_cast<float>(i); })
      | vws::transform(std::bind(wavelength_to_phase, _1, true)), input_phase.begin());

    // Expand out of range values 
    // auto phase  = (BSpec() << input_phase.head<1>(), input_phase, input_phase.tail<1>()).finished();
    auto phase  = (BSpec() << -std::numbers::pi_v<float>, input_phase, 0.0)
                  .finished().cast<double>().eval();
    auto signal = (BSpec() << input_signal.head<1>(), input_signal, input_signal.tail<1>())
                  .finished().cast<double>().eval();

    // Initialize real/complex parts of value as all zeroes
    CMom moments = CMom::Zero();

    // Handle integration
    for (uint i = 0; i < BSpec::RowsAtCompileTime; ++i) {
      guard_continue(phase[i] < phase[i + 1]);

      auto gradient = (signal[i + 1] - signal[i]) / (phase[i + 1]  - phase[i]);      
      auto y_inscpt = signal[i] - gradient * phase[i];

      for (uint j = 1; j < moment_samples + 1; ++j) {
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

    moments *= 0.5 * std::numbers::inv_pi_v<float>;
    return (2.0 * moments.real()).cast<float>().eval();
  }

  Spec moments_to_spectrum(const Moments &bm) {
    met_trace();

    using namespace std::complex_literals;
    using namespace std::placeholders;

    // Get vector of wavelength phases // TODO extract from computation
    eig::Array<double, wavelength_samples, 1> input_phase;
    rng::copy(vws::iota(0u, wavelength_samples) 
      // | vws::transform(wavelength_at_index)
      | vws::transform([](uint i) {
        return wavelength_min + wavelength_ssize * static_cast<float>(i); })
      | vws::transform(std::bind(wavelength_to_phase, _1, true)), input_phase.begin());

    // Get exponential moments
    auto em = detail::bounded_to_exponential_moments(bm);

    auto toepl_first_col = (em * 0.5 * std::numbers::inv_pi).eval();
    toepl_first_col[0]   = 2.0 * toepl_first_col[0].real();

    auto eval_polynm   = detail::levisons_algorithm(toepl_first_col);
    auto point_in_disk = (1.0i * input_phase).exp().eval();

    em[0] *= 2.0;

    auto trf = detail::herglotz_transform(point_in_disk, em, eval_polynm);
    
    return (trf.arg() * std::numbers::inv_pi + 0.5).cast<float>().eval();
  }
} // namespace met