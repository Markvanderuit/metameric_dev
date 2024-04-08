#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/constraints.hpp>
#include <metameric/core/matching.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/moments.hpp>
#include <metameric/core/ranges.hpp>
#include <metameric/core/spectrum.hpp>
#include <metameric/core/utility.hpp>
#include <complex>

namespace met {
  // Templated moment code
  using MomentsR8 = eig::Array<float, 8, 1>;
  using MomentsC8 = eig::Array<eig::scomplex, 8, 1>;
  
  namespace peters {
    MomentsC8 trigonometricToExponentialMomentsReal8(const MomentsR8 &pTrigonometricMoment){
      MomentsC8 pOutExponentialMoment;

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

      return pOutExponentialMoment;
    }

    MomentsC8 levinsonsAlgorithm8(const MomentsC8 &pFirstColumn) {
      MomentsC8 pOutSolution = MomentsC8::Zero();

      pOutSolution[0]=std::complex<float>(1.0f/(pFirstColumn[0].real()));

      std::complex<float> dotProduct;
      MomentsC8           flippedSolution;
      float factor;
      
      dotProduct=pOutSolution[0].real()*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(-flippedSolution[1].real()*dotProduct);

      dotProduct=pOutSolution[0].real()*pFirstColumn[2]+pOutSolution[1]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::conj(pOutSolution[1]);
      flippedSolution[2]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution[1]*dotProduct);
      pOutSolution[2]=factor*(-flippedSolution[2].real()*dotProduct);

      dotProduct=pOutSolution[0].real()*pFirstColumn[3]+pOutSolution[1]*pFirstColumn[2]+pOutSolution[2]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::conj(pOutSolution[2]);
      flippedSolution[2]=std::conj(pOutSolution[1]);
      flippedSolution[3]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution[1]*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution[2]*dotProduct);
      pOutSolution[3]=factor*(-flippedSolution[3].real()*dotProduct);

      dotProduct=pOutSolution[0].real()*pFirstColumn[4]+pOutSolution[1]*pFirstColumn[3]+pOutSolution[2]*pFirstColumn[2]+pOutSolution[3]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::conj(pOutSolution[3]);
      flippedSolution[2]=std::conj(pOutSolution[2]);
      flippedSolution[3]=std::conj(pOutSolution[1]);
      flippedSolution[4]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution[1]*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution[2]*dotProduct);
      pOutSolution[3]=factor*(pOutSolution[3]-flippedSolution[3]*dotProduct);
      pOutSolution[4]=factor*(-flippedSolution[4].real()*dotProduct);

      dotProduct=pOutSolution[0].real()*pFirstColumn[5]+pOutSolution[1]*pFirstColumn[4]+pOutSolution[2]*pFirstColumn[3]+pOutSolution[3]*pFirstColumn[2]+pOutSolution[4]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::conj(pOutSolution[4]);
      flippedSolution[2]=std::conj(pOutSolution[3]);
      flippedSolution[3]=std::conj(pOutSolution[2]);
      flippedSolution[4]=std::conj(pOutSolution[1]);
      flippedSolution[5]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      pOutSolution[1]=factor*(pOutSolution[1]-flippedSolution[1]*dotProduct);
      pOutSolution[2]=factor*(pOutSolution[2]-flippedSolution[2]*dotProduct);
      pOutSolution[3]=factor*(pOutSolution[3]-flippedSolution[3]*dotProduct);
      pOutSolution[4]=factor*(pOutSolution[4]-flippedSolution[4]*dotProduct);
      pOutSolution[5]=factor*(-flippedSolution[5].real()*dotProduct);
      
      dotProduct=pOutSolution[0].real()*pFirstColumn[6]+pOutSolution[1]*pFirstColumn[5]+pOutSolution[2]*pFirstColumn[4]+pOutSolution[3]*pFirstColumn[3]+pOutSolution[4]*pFirstColumn[2]+pOutSolution[5]*pFirstColumn[1];
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::conj(pOutSolution[5]);
      flippedSolution[2]=std::conj(pOutSolution[4]);
      flippedSolution[3]=std::conj(pOutSolution[3]);
      flippedSolution[4]=std::conj(pOutSolution[2]);
      flippedSolution[5]=std::conj(pOutSolution[1]);
      flippedSolution[6]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      for (uint j = 1; j < 6; ++j) {
        pOutSolution[j]=factor*(pOutSolution[j]-flippedSolution[j]*dotProduct);
      }
      pOutSolution[6]=factor*(-flippedSolution[6].real()*dotProduct);

      dotProduct=pOutSolution[0].real()*pFirstColumn[7]; //+pOutSolution[1]*pFirstColumn[6]+pOutSolution[2]*pFirstColumn[5]+pOutSolution[3]*pFirstColumn[4]+pOutSolution[4]*pFirstColumn[3]+pOutSolution[5]*pFirstColumn[2]+pOutSolution[6]*pFirstColumn[1];
      for (uint j = 1; j < 7; ++j)
        dotProduct += pOutSolution[j] * pFirstColumn[7 - j];
        
      factor=1.0f/(1.0f-std::norm(dotProduct));
      flippedSolution[1]=std::conj(pOutSolution[6]);
      flippedSolution[2]=std::conj(pOutSolution[5]);
      flippedSolution[3]=std::conj(pOutSolution[4]);
      flippedSolution[4]=std::conj(pOutSolution[3]);
      flippedSolution[5]=std::conj(pOutSolution[2]);
      flippedSolution[6]=std::conj(pOutSolution[1]);
      flippedSolution[7]=std::complex<float>(pOutSolution[0].real());
      pOutSolution[0]=std::complex<float>(factor*pOutSolution[0].real());
      for (uint j = 1; j < 7; ++j)
        pOutSolution[j]=factor*(pOutSolution[j]-flippedSolution[j]*dotProduct);
      pOutSolution[7]=factor*(-flippedSolution[7].real()*dotProduct);

      return pOutSolution;
    }

    std::pair<MomentsC8, MomentsC8> prepareReflectanceSpectrumReal8(const MomentsR8 &pTrigonometricMoment){
      MomentsC8 pOutExponentialMoment = trigonometricToExponentialMomentsReal8(pTrigonometricMoment);
      MomentsC8 pOutEvaluationPolynomial = levinsonsAlgorithm8(pOutExponentialMoment);
      pOutEvaluationPolynomial[0]=6.28318531f*pOutEvaluationPolynomial[0];
      pOutEvaluationPolynomial[1]=6.28318531f*pOutEvaluationPolynomial[1];
      pOutEvaluationPolynomial[2]=6.28318531f*pOutEvaluationPolynomial[2];
      pOutEvaluationPolynomial[3]=6.28318531f*pOutEvaluationPolynomial[3];
      pOutEvaluationPolynomial[4]=6.28318531f*pOutEvaluationPolynomial[4];
      pOutEvaluationPolynomial[5]=6.28318531f*pOutEvaluationPolynomial[5];
      pOutEvaluationPolynomial[6]=6.28318531f*pOutEvaluationPolynomial[6];
      pOutEvaluationPolynomial[7]=6.28318531f*pOutEvaluationPolynomial[7];
      return { pOutExponentialMoment, pOutEvaluationPolynomial };
    }
    
    std::complex<float> evaluateFastHerglotzTransform8(const std::complex<float> &circlePoint, const MomentsC8 &pExponentialMoment, const MomentsC8 &pEvaluationPolynomial){
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

    float evaluateReflectanceSpectrum8(const float phase,const MomentsC8 &pExponentialMoment, const MomentsC8 &pEvaluationPolynomial){
      std::complex<float> circlePoint = { std::cos(phase), std::sin(phase) };
      std::complex<float> herglotzTransform = evaluateFastHerglotzTransform8(circlePoint,pExponentialMoment,pEvaluationPolynomial);
      return std::atan2(herglotzTransform.imag(), herglotzTransform.real()) * 0.318309886f + 0.5f;
    }
  } // namespace peters

  namespace detail {
    MomentsC8 trigonometric_to_exponential_moments(const MomentsR8 &bm) {
      MomentsC8 em = MomentsC8::Zero();
      
      float zeroeth_phase = bm[0] * std::numbers::pi_v<float> - 0.5f * std::numbers::pi_v<float>;
      em[0] = .0795774715f * eig::scomplex(std::cos(zeroeth_phase), std::sin(zeroeth_phase));
      
      for (uint i = 1; i < MomentsC8::RowsAtCompileTime; ++i) {
        for (uint j = 0; j < i; ++j)
          em[i] += bm[i - j] 
                 * em[j] 
                 * eig::scomplex(0.f, static_cast<float>(i - j));

        em[i] *= 2.f 
               * std::numbers::pi_v<float> 
               / static_cast<float>(i);
      } // for (uint i)

      em[0] = 2.0f * em[0];

      return em;
    }

    MomentsC8 levinsons_algorithm(const MomentsC8 &fc) { // first column
      MomentsC8 rm = MomentsC8::Zero();
      rm[0] = eig::scomplex(1.f / fc[0].real());

      for (uint i = 1; i < MomentsC8::RowsAtCompileTime; ++i) {
        eig::scomplex dp = rm[0].real() * fc[i];
        for (uint j = 1; j < i; ++j)
          dp += rm[j] * fc[i - j];
        float factor = 1.f / (1.f - std::norm(dp));
        
        MomentsC8 flipped_solution;
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

    std::pair<MomentsC8, MomentsC8> prepare_reflectance(const MomentsR8 &bm) {
      auto em = trigonometric_to_exponential_moments(bm);
      auto pm = levinsons_algorithm(em);
      for (uint i = 0; i < MomentsC8::RowsAtCompileTime; ++i)
        pm[i] = 2.f * std::numbers::pi_v<float> * pm[i];
      return { em, pm };
    }

    eig::scomplex fast_herglotz_trf(const eig::scomplex &circle_point, const MomentsC8 &em, const MomentsC8 &pm) {
      eig::scomplex conj_circle_point = std::conj(circle_point);
      
      MomentsC8 polynomial;
      polynomial[0] = pm[0].real();
      for (uint j = 1; j < MomentsC8::RowsAtCompileTime; ++j)
        polynomial[j] = pm[j] + polynomial[j - 1] * conj_circle_point;

      eig::scomplex dp = 0.0;
      for (uint j = 1; j < MomentsC8::RowsAtCompileTime; ++j)
        dp += polynomial[MomentsC8::RowsAtCompileTime - j - 1] * em[j];

      return em[0] + 2.f * dp / polynomial[MomentsC8::RowsAtCompileTime - 1];
    }
    
    float evaluate_reflectance(float phase,const MomentsC8 &em, const MomentsC8 &pm){
      eig::scomplex circle_point = { std::cos(phase), std::sin(phase) };
      auto herglots_trf = fast_herglotz_trf(circle_point, em, pm);
      return std::atan2(herglots_trf.imag(), herglots_trf.real()) * std::numbers::inv_pi_v<float> + 0.5f;
    }
  } // namespace detail
  
  TEST_CASE("Moment template rewrite") {
    MomentsR8 trigonometric_moments = { 0.53361477, 0.03668047, -0.02211483, -0.04177091, -0.04679692,  0.01339208, 0.06915859,  0.02681544 };

    SECTION("Trigonometric to exponential") {
      auto a = peters::trigonometricToExponentialMomentsReal8(trigonometric_moments);
      auto b = detail::trigonometric_to_exponential_moments(trigonometric_moments);
      CHECK(a.isApprox(b));
    }

    SECTION("Levinson's algorithm") {
      auto em = peters::trigonometricToExponentialMomentsReal8(trigonometric_moments);
      auto a = peters::levinsonsAlgorithm8(em);
      auto b = detail::levinsons_algorithm(em);
      CHECK(a.isApprox(b));
    }

    SECTION("Prepare reflectance spectrum") {
      auto [em_a, pm_a] = peters::prepareReflectanceSpectrumReal8(trigonometric_moments);
      auto [em_b, pm_b] = detail::prepare_reflectance(trigonometric_moments);
      CHECK(em_a.isApprox(em_b));
      CHECK(pm_a.isApprox(pm_b));
    }

    SECTION("Herglotz transform") {
      auto [em, pm] = peters::prepareReflectanceSpectrumReal8(trigonometric_moments);
      float phase = -0.15 * std::numbers::pi_v<float>;
      eig::scomplex circle_point = { std::cos(phase), std::sin(phase) };
      auto a = peters::evaluateFastHerglotzTransform8(circle_point, em, pm);
      auto b = detail::fast_herglotz_trf(circle_point, em, pm);
      CHECK(a == b);
    }

    SECTION("Evaluate reflectance") {
      auto [em, pm] = peters::prepareReflectanceSpectrumReal8(trigonometric_moments);
      float phase = -0.15 * std::numbers::pi_v<float>;
      auto a = peters::evaluateReflectanceSpectrum8(phase, em, pm);
      auto b = detail::evaluate_reflectance(phase, em, pm);
      CHECK(a == b);
    }
  };
} // namespace met