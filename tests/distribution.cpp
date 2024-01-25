#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/spectrum.hpp>

using namespace met;

constexpr static uint  n_samples = 128;
constexpr static float eps       = 0.01f;

TEST_CASE("Distribution 1D") {
  // PCGSampler     sampler(5);
  UniformSampler sampler(5);

  SECTION("Sampler") {
    float value = 0.f;
    for (uint i = 0; i < n_samples; ++i) {
      float new_value = sampler.next_1d();

      // Incremental average to avoid precision problems
      value += (new_value - value) / static_cast<float>(i + 1);
    }
    
    float expected = 0.5f; // Mean should go to 0.5
    REQUIRE_THAT(value, Catch::Matchers::WithinAbs(expected, eps));
  } // SECTION

  SECTION("Uniform distribution (Discrete)") {
    Spec s = 1.f;
    Distribution cdf(cnt_span<float>(s));

    float value = 0.f;
    for (uint i = 0; i < n_samples; ++i) {
      uint  sample    = cdf.sample_discrete(sampler.next_1d());
      float pdf       = cdf.pdf_discrete(sample);
      float new_value = s[sample] / pdf;

      // Incremental average to avoid precision problems
      value += (new_value - value) / static_cast<float>(i + 1);
    }

    float expected = s.sum();
    REQUIRE_THAT(value, Catch::Matchers::WithinAbs(expected, eps));
  } // SECTION

  SECTION("Skewed distribution (Discrete)") {
    Spec s = models::emitter_cie_d65;
    Distribution cdf(cnt_span<float>(s));

    float value = 0.f;
    for (uint i = 0; i < n_samples; ++i) {
      float u         = sampler.next_1d();
      uint  sample    = cdf.sample_discrete(u);
      float pdf       = cdf.pdf_discrete(sample);
      float new_value = pdf == 0.f 
                      ? 0.f 
                      : s[sample] / pdf;

      // Incremental average to avoid precision problems
      value += (new_value - value) / static_cast<float>(i + 1);
    }

    float expected = s.sum();
    REQUIRE_THAT(value, Catch::Matchers::WithinAbs(expected, eps));
  } // SECTION
  
  SECTION("Skewed distribution (Piecewise linear)") {
    Spec s = models::emitter_cie_fl11;
    Distribution cdf(cnt_span<float>(s));

    // We test for weighted uniformity, so discard the initial value
    // to improve precision
    // s = 1.f;

    float expected = s.sum() / static_cast<float>(s.size());
    float value = 0.f;

    for (uint i = 0; i < n_samples; ++i) {
      float sample = cdf.sample(sampler.next_1d());
      float pdf    = cdf.pdf(sample);
      
      uint  size  = wavelength_samples - 1;
      uint  index = static_cast<uint>(sample * size);
      float alpha = sample * size - static_cast<float>(index);

      float new_value = alpha == 0.f || index >= size
                      ? s[index]
                      : std::lerp(s[index], s[index + 1], alpha);
      new_value = pdf == 0.f ? 0.f : new_value / pdf;
      
      // Incremental average to avoid precision problems
      value += (new_value - value) / static_cast<float>(i + 1);
    }
    
    REQUIRE_THAT(value, Catch::Matchers::WithinAbs(expected, eps));
  } // SECTION

} // TEST_CASE