#ifndef DISTRIBUTION_GLSL_GUARD
#define DISTRIBUTION_GLSL_GUARD

#include <spectrum.glsl>

// Return value for sample_distr(u) functions
struct DistributionSample {
  float f;
  float pdf;
};

#define declare_distr_sampler(name, distr)                       \
  uint sample_##name##_discrete(in float u) {                    \
    int i = 0;                                                   \
    while (u > distr.cdf[i] && i < distr.cdf.length() - 1)       \
      i++;                                                       \
    i -= 1;                                                      \
    if (distr.func[i] == 0.f && i < distr.func.length() - 1)     \
      i++;                                                       \
    return uint(clamp(i, 0, distr.cdf.length() - 2));            \
  }                                                              \
                                                                 \
  float pdf_##name(in float sample_1d) {                         \
    uint  i  = uint(sample_1d * float(distr.cdf.length() - 2));  \
    float a  = sample_1d * float(distr.cdf.length() - 2)         \
             - float(i);                                         \
    if (a == 0.f) {                                              \
      return distr.func[i] / distr.func_sum;                     \
    } else {                                                     \
      return mix(distr.func[i    ] / distr.func_sum,             \
                 distr.func[i + 1] / distr.func_sum ,            \
                 a);                                             \
    }                                                            \
  }                                                              \
                                                                 \
  DistributionSample sample_##name(in float u) {                 \
    DistributionSample ds;                                       \
                                                                 \
    uint  i = sample_##name##_discrete(u);                       \
    float d = distr.cdf[i + 1] - distr.cdf[i];                   \
    if (d == 0.f) {                                              \
      ds.f = float(i) / float(distr.cdf.length() - 1);           \
    } else {                                                     \
      float a = (u - distr.cdf[i]) / d;                          \
      ds.f = (float(i) + a) / float(distr.cdf.length() - 1);     \
    }                                                            \
    ds.pdf = pdf_##name(ds.f);                                   \
    return ds;                                                   \
  }                                                              \

#endif // DISTRIBUTION_GLSL_GUARD