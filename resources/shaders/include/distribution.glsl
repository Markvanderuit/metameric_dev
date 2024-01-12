#ifndef DISTRIBUTION_GLSL_GUARD
#define DISTRIBUTION_GLSL_GUARD

#include <spectrum.glsl>

// Return value for sample_distr(u) functions
struct DistributionSample {
  float f;
  float pdf;
};

layout(binding = 0) uniform b_buff_distr {
  float pdf[wavelength_samples];
  float cdf[wavelength_samples + 1];
} buff_distr;

#define declare_distr_sampler(name, distr)                 \
  uint sample_##name##_discrete(in float u) {              \
    int i = 0;                                             \
    while (u > distr.cdf[i] && i < distr.cdf.length() - 1) \
      i++;                                                 \
    i -= 1;                                                \
    return uint(clamp(i, 0, distr.pdf.length() - 1));      \
  }                                                        \
                                                           \
float pdf_##name(in float sample_1d) {                     \
  uint i    = uint(sample_1d);                             \
  float pdf = distr.pdf[i];                                \
                                                           \
  float a = sample_1d - float(i);                          \
  if (a != 0.f && a < distr.pdf.length() - 1)              \
    pdf += a * distr.pdf[i + 1];                           \
                                                           \
  return pdf;                                              \
}                                                          \
                                                           \
DistributionSample sample_##name(in float u) {             \
  DistributionSample ds;                                   \
                                                           \
  uint i = sample_##name##_discrete(u);                    \
  ds.f = float(i);                                         \
                                                           \
  float d = distr.cdf[i + 1] - distr.cdf[i];               \
  if (d != 0.f)                                            \
    ds.f = float(i) + (u - distr.cdf[i]) / d;              \
                                                           \
  ds.pdf = pdf_##name(ds.f);                               \
                                                           \
  return ds;                                               \
}                                                          \

#endif // DISTRIBUTION_GLSL_GUARD