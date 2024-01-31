#ifndef DISTRIBUTION_GLSL_GUARD
#define DISTRIBUTION_GLSL_GUARD

// Return values for sample_distr(u) functions
struct DistributionSampleContinuous {
  float f;
  float pdf;
};
struct DistributionSampleDiscrete {
  uint  i;
  float pdf;
};

#define declare_distr_sampler(name, distr, distr_size)                  \
  uint sample_##name##_base(in float u) {                               \
    int i = 0;                                                          \
    while (u > distr.cdf[i] && i < distr_size)                          \
      i++;                                                              \
    i -= 1;                                                             \
    if (distr.func[i] == 0.f && i < distr_size - 1)                     \
      i++;                                                              \
    return uint(clamp(i, 0, distr_size - 1));                           \
  }                                                                     \
                                                                        \
  float pdf_##name##_discrete(in uint i) {                              \
    return distr.func[i] / float(distr_size);                           \
  }                                                                     \
                                                                        \
  float pdf_##name(in float sample_1d) {                                \
    uint  i  = uint(sample_1d * float(distr_size - 1));                 \
    float a  = sample_1d * float(distr_size - 1)                        \
             - float(i);                                                \
    if (a == 0.f) {                                                     \
      return distr.func[i];                                             \
    } else {                                                            \
      return mix(distr.func[i    ],                                     \
                 distr.func[i + 1] ,                                    \
                 a);                                                    \
    }                                                                   \
  }                                                                     \
                                                                        \
  DistributionSampleDiscrete sample_##name##_discrete(in float u) {     \
    DistributionSampleDiscrete ds;                                      \
    ds.i   = sample_##name##_base(u);                                   \
    ds.pdf = distr.func[ds.i] / float(distr_size);                      \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  DistributionSampleContinuous sample_##name##_continuous(in float u) { \
    DistributionSampleContinuous ds;                                    \
                                                                        \
    uint  i = sample_##name##_base(u);                                  \
    float d = distr.cdf[i + 1] - distr.cdf[i];                          \
    if (d == 0.f) {                                                     \
      ds.f = float(i) / float(distr_size);                              \
    } else {                                                            \
      float a = (u - distr.cdf[i]) / d;                                 \
      ds.f = (float(i) + a) / float(distr_size);                        \
    }                                                                   \
    ds.pdf = pdf_##name(ds.f);                                          \
    return ds;                                                          \
  }                                                                     \

#define declare_distr_sampler_default(name, distr, distr_size)          \
  float pdf_##name##_discrete(in uint i) {                              \
    return 1.f;                                                         \
  }                                                                     \
                                                                        \
  float pdf_##name(in float sample_1d) {                                \
    return 1.f;                                                         \
  }                                                                     \
                                                                        \
  DistributionSampleDiscrete sample_##name##_discrete(in float u) {     \
    DistributionSampleDiscrete ds;                                      \
    ds.i   = 0u;                                                        \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  DistributionSampleContinuous sample_##name##_continuous(in float u) { \
    DistributionSampleContinuous ds;                                    \
    ds.f   = 0.f;                                                       \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }                                                                     \

#endif // DISTRIBUTION_GLSL_GUARD