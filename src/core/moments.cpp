#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/utility.hpp>
#include <complex>

namespace met {
  using SMom  = eig::Array<eig::scomplex, moment_samples + 1, 1>;
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

  namespace peters {
    void trigonometricToExponentialMomentsReal8(std::complex<float> pOutExponentialMoment[8],const float pTrigonometricMoment[8]){
      float zerothMomentPhase=3.14159265f*pTrigonometricMoment[0]-1.57079633f;
      pOutExponentialMoment[0]=std::complex<float>(std::cos(zerothMomentPhase),std::sin(zerothMomentPhase));
      pOutExponentialMoment[0]=0.0795774715f*pOutExponentialMoment[0];
      pOutExponentialMoment[1]=pTrigonometricMoment[1]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0];
      pOutExponentialMoment[2]=pTrigonometricMoment[2]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0]+pTrigonometricMoment[1]*std::complex<float>(0.0f,3.14159265f)*pOutExponentialMoment[1];
      pOutExponentialMoment[3]=pTrigonometricMoment[3]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0]+pTrigonometricMoment[2]*std::complex<float>(0.0f,4.1887902f)*pOutExponentialMoment[1]+pTrigonometricMoment[1]*std::complex<float>(0.0f,2.0943951f)*pOutExponentialMoment[2];
      pOutExponentialMoment[4]=pTrigonometricMoment[4]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0]+pTrigonometricMoment[3]*std::complex<float>(0.0f,4.71238898f)*pOutExponentialMoment[1]+pTrigonometricMoment[2]*std::complex<float>(0.0f,3.14159265f)*pOutExponentialMoment[2]+pTrigonometricMoment[1]*std::complex<float>(0.0f,1.57079633f)*pOutExponentialMoment[3];
      pOutExponentialMoment[5]=pTrigonometricMoment[5]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0]+pTrigonometricMoment[4]*std::complex<float>(0.0f,5.02654825f)*pOutExponentialMoment[1]+pTrigonometricMoment[3]*std::complex<float>(0.0f,3.76991118f)*pOutExponentialMoment[2]+pTrigonometricMoment[2]*std::complex<float>(0.0f,2.51327412f)*pOutExponentialMoment[3]+pTrigonometricMoment[1]*std::complex<float>(0.0f,1.25663706f)*pOutExponentialMoment[4];
      pOutExponentialMoment[6]=pTrigonometricMoment[6]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0]+pTrigonometricMoment[5]*std::complex<float>(0.0f,5.23598776f)*pOutExponentialMoment[1]+pTrigonometricMoment[4]*std::complex<float>(0.0f,4.1887902f)*pOutExponentialMoment[2]+pTrigonometricMoment[3]*std::complex<float>(0.0f,3.14159265f)*pOutExponentialMoment[3]+pTrigonometricMoment[2]*std::complex<float>(0.0f,2.0943951f)*pOutExponentialMoment[4]+pTrigonometricMoment[1]*std::complex<float>(0.0f,1.04719755f)*pOutExponentialMoment[5];
      pOutExponentialMoment[7]=pTrigonometricMoment[7]*std::complex<float>(0.0f,6.28318531f)*pOutExponentialMoment[0]+pTrigonometricMoment[6]*std::complex<float>(0.0f,5.38558741f)*pOutExponentialMoment[1]+pTrigonometricMoment[5]*std::complex<float>(0.0f,4.48798951f)*pOutExponentialMoment[2]+pTrigonometricMoment[4]*std::complex<float>(0.0f,3.5903916f)*pOutExponentialMoment[3]+pTrigonometricMoment[3]*std::complex<float>(0.0f,2.6927937f)*pOutExponentialMoment[4]+pTrigonometricMoment[2]*std::complex<float>(0.0f,1.7951958f)*pOutExponentialMoment[5]+pTrigonometricMoment[1]*std::complex<float>(0.0f,0.897597901f)*pOutExponentialMoment[6];
      pOutExponentialMoment[0]=2.0f*pOutExponentialMoment[0];
    }

    void levinsonsAlgorithm8(std::complex<float> pOutSolution[8],const std::complex<float> pFirstColumn[8]){
      pOutSolution[0]=std::complex<float>(1.0f/(pFirstColumn[0].real()));
      std::complex<float> dotProduct;
      std::complex<float> flippedSolution1;
      std::complex<float> flippedSolution2;
      std::complex<float> flippedSolution3;
      std::complex<float> flippedSolution4;
      std::complex<float> flippedSolution5;
      std::complex<float> flippedSolution6;
      std::complex<float> flippedSolution7;
      float factor;
      dotProduct=pOutSolution[0].real()*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(-flippedSolution1.real()*dotProduct);
      dotProduct=pOutSolution[0].real()*pFirstColumn[2]+pOutSolution[1]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::conj(pOutSolution[1]);
      flippedSolution2=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution1*dotProduct);
      pOutSolution[2]=factor*(-flippedSolution2.real()*dotProduct);
      dotProduct=pOutSolution[0].real()*pFirstColumn[3]+pOutSolution[1]*pFirstColumn[2]+pOutSolution[2]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::conj(pOutSolution[2]);
      flippedSolution2=std::conj(pOutSolution[1]);
      flippedSolution3=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution1*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution2*dotProduct);
      pOutSolution[3]=factor*(-flippedSolution3.real()*dotProduct);
      dotProduct=pOutSolution[0].real()*pFirstColumn[4]+pOutSolution[1]*pFirstColumn[3]+pOutSolution[2]*pFirstColumn[2]+pOutSolution[3]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::conj(pOutSolution[3]);
      flippedSolution2=std::conj(pOutSolution[2]);
      flippedSolution3=std::conj(pOutSolution[1]);
      flippedSolution4=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution1*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution2*dotProduct);
      pOutSolution[3]=factor*(pOutSolution[3]-flippedSolution3*dotProduct);
      pOutSolution[4]=factor*(-flippedSolution4.real()*dotProduct);
      dotProduct=pOutSolution[0].real()*pFirstColumn[5]+pOutSolution[1]*pFirstColumn[4]+pOutSolution[2]*pFirstColumn[3]+pOutSolution[3]*pFirstColumn[2]+pOutSolution[4]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::conj(pOutSolution[4]);
      flippedSolution2=std::conj(pOutSolution[3]);
      flippedSolution3=std::conj(pOutSolution[2]);
      flippedSolution4=std::conj(pOutSolution[1]);
      flippedSolution5=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution1*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution2*dotProduct);
      pOutSolution[3]=factor*(pOutSolution[3]-flippedSolution3*dotProduct);
      pOutSolution[4]=factor*(pOutSolution[4]-flippedSolution4*dotProduct);
      pOutSolution[5]=factor*(-flippedSolution5.real()*dotProduct);
      dotProduct=pOutSolution[0].real()*pFirstColumn[6]+pOutSolution[1]*pFirstColumn[5]+pOutSolution[2]*pFirstColumn[4]+pOutSolution[3]*pFirstColumn[3]+pOutSolution[4]*pFirstColumn[2]+pOutSolution[5]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::conj(pOutSolution[5]);
      flippedSolution2=std::conj(pOutSolution[4]);
      flippedSolution3=std::conj(pOutSolution[3]);
      flippedSolution4=std::conj(pOutSolution[2]);
      flippedSolution5=std::conj(pOutSolution[1]);
      flippedSolution6=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution1*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution2*dotProduct);
      pOutSolution[3]=factor*(pOutSolution[3]-flippedSolution3*dotProduct);
      pOutSolution[4]=factor*(pOutSolution[4]-flippedSolution4*dotProduct);
      pOutSolution[5]=factor*(pOutSolution[5]-flippedSolution5*dotProduct);
      pOutSolution[6]=factor*(-flippedSolution6.real()*dotProduct);
      dotProduct=pOutSolution[0].real()*pFirstColumn[7]+pOutSolution[1]*pFirstColumn[6]+pOutSolution[2]*pFirstColumn[5]+pOutSolution[3]*pFirstColumn[4]+pOutSolution[4]*pFirstColumn[3]+pOutSolution[5]*pFirstColumn[2]+pOutSolution[6]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution1=std::conj(pOutSolution[6]);
      flippedSolution2=std::conj(pOutSolution[5]);
      flippedSolution3=std::conj(pOutSolution[4]);
      flippedSolution4=std::conj(pOutSolution[3]);
      flippedSolution5=std::conj(pOutSolution[2]);
      flippedSolution6=std::conj(pOutSolution[1]);
      flippedSolution7=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution1*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution2*dotProduct);
      pOutSolution[3]=factor*(pOutSolution[3]-flippedSolution3*dotProduct);
      pOutSolution[4]=factor*(pOutSolution[4]-flippedSolution4*dotProduct);
      pOutSolution[5]=factor*(pOutSolution[5]-flippedSolution5*dotProduct);
      pOutSolution[6]=factor*(pOutSolution[6]-flippedSolution6*dotProduct);
      pOutSolution[7]=factor*(-flippedSolution7.real()*dotProduct);
    }

    std::complex<float> evaluateFastHerglotzTransform8(const std::complex<float> circlePoint,const std::complex<float> pExponentialMoment[8],const std::complex<float> pEvaluationPolynomial[8]){
      std::complex<float> conjCirclePoint=std::conj(circlePoint);
      float polynomial7=pEvaluationPolynomial[0].real();
      std::complex<float> polynomial6=pEvaluationPolynomial[1]+polynomial7*conjCirclePoint;
      std::complex<float> polynomial5=pEvaluationPolynomial[2]+conjCirclePoint*polynomial6;
      std::complex<float> polynomial4=pEvaluationPolynomial[3]+conjCirclePoint*polynomial5;
      std::complex<float> polynomial3=pEvaluationPolynomial[4]+conjCirclePoint*polynomial4;
      std::complex<float> polynomial2=pEvaluationPolynomial[5]+conjCirclePoint*polynomial3;
      std::complex<float> polynomial1=pEvaluationPolynomial[6]+conjCirclePoint*polynomial2;
      std::complex<float> polynomial0=pEvaluationPolynomial[7]+conjCirclePoint*polynomial1;
      std::complex<float> dotProduct=polynomial1*pExponentialMoment[1]+polynomial2*pExponentialMoment[2]+polynomial3*pExponentialMoment[3]+polynomial4*pExponentialMoment[4]+polynomial5*pExponentialMoment[5]+polynomial6*pExponentialMoment[6]+polynomial7*pExponentialMoment[7];
      return pExponentialMoment[0]+2.0f*(dotProduct)/(polynomial0);
    }

    void prepareReflectanceSpectrumReal8(std::complex<float> pOutExponentialMoment[8],std::complex<float> pOutEvaluationPolynomial[8],const float pTrigonometricMoment[8]){
      trigonometricToExponentialMomentsReal8(pOutExponentialMoment,pTrigonometricMoment);
      levinsonsAlgorithm8(pOutEvaluationPolynomial,pOutExponentialMoment);
      pOutEvaluationPolynomial[0]=6.28318531f*pOutEvaluationPolynomial[0];
      pOutEvaluationPolynomial[1]=6.28318531f*pOutEvaluationPolynomial[1];
      pOutEvaluationPolynomial[2]=6.28318531f*pOutEvaluationPolynomial[2];
      pOutEvaluationPolynomial[3]=6.28318531f*pOutEvaluationPolynomial[3];
      pOutEvaluationPolynomial[4]=6.28318531f*pOutEvaluationPolynomial[4];
      pOutEvaluationPolynomial[5]=6.28318531f*pOutEvaluationPolynomial[5];
      pOutEvaluationPolynomial[6]=6.28318531f*pOutEvaluationPolynomial[6];
      pOutEvaluationPolynomial[7]=6.28318531f*pOutEvaluationPolynomial[7];
    }

    float evaluateReflectanceSpectrum8(const float phase,const std::complex<float> pExponentialMoment[8],const std::complex<float> pEvaluationPolynomial[8]){
      std::complex<float> circlePoint;
      circlePoint=std::complex<float>(std::cos(phase),std::sin(phase));
      std::complex<float> herglotzTransform;
      herglotzTransform=evaluateFastHerglotzTransform8(circlePoint,pExponentialMoment,pEvaluationPolynomial);
      return std::atan2(herglotzTransform.imag(),herglotzTransform.real())*0.318309886f+0.5f;
    }
    
    // Compute a discrete spectral reflectance given trigonometric moments
    Spec moments_to_spectrum(const Moments &m) {
      using namespace std::complex_literals;
      using namespace std::placeholders;

      // Get vector of wavelength phases // TODO extract from computation
      eig::Array<double, wavelength_samples, 1> input_phase;
      rng::copy(vws::iota(0u, wavelength_samples) 
        | vws::transform(wavelength_at_index)
        | vws::transform(std::bind(wavelength_to_phase, _1, true)), input_phase.begin());

      SMom em, ep;
      prepareReflectanceSpectrumReal8(em.data(), ep.data(), m.data());

      Spec s;
      rng::transform(input_phase, s.begin(), [&](float f) {
        return evaluateReflectanceSpectrum8(f, em.data(), ep.data());
      });

      return s;
    }
  } // namespace peters

  // TODO; add warp
  Moments spectrum_to_moments(const Spec &input_signal) {
    met_trace();

    using namespace std::complex_literals;
    using namespace std::placeholders;
    using BSpec = eig::Array<float, wavelength_samples + 2, 1>;

    // Get vector of wavelength phases // TODO extract from computation
    Spec input_phase;
    rng::copy(vws::iota(0u, wavelength_samples) 
      | vws::transform(wavelength_at_index)
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
      | vws::transform(wavelength_at_index)
      | vws::transform(std::bind(wavelength_to_phase, _1, true)), input_phase.begin());

    // Get exponential moments
    auto em = detail::bounded_to_exponential_moments(bm);

    auto toepl_first_col = (em * 0.5 * std::numbers::inv_pi).eval();
    toepl_first_col[0]   = 2.0 * toepl_first_col[0].real();

    auto eval_polynm   = detail::levisons_algorithm(toepl_first_col);
    auto point_in_disk = (1.0i * input_phase).exp().eval();

    auto trf = detail::herglotz_transform(point_in_disk, 2.0 * em, eval_polynm);    
    return (trf.arg() * std::numbers::inv_pi + 0.5).cast<float>().eval();
  }
} // namespace met