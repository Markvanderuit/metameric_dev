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

#define declare_distr_sampler(name, distr, distr_len)                   \
  uint sample_##name##_base(in float u) {                               \
    int i = 0;                                                          \
    while (u > distr.cdf[i] && i < distr_len)                           \
      i++;                                                              \
    i -= 1;                                                             \
    if (distr.func[i] == 0.f && i < distr_len - 1)                      \
      i++;                                                              \
    return uint(clamp(i, 0, distr_len - 1));                            \
  }                                                                     \
                                                                        \
  float pdf_##name##_discrete(in uint i) {                              \
    return distr.func[i];                                               \
  }                                                                     \
                                                                        \
  float pdf_##name(in float sample_1d) {                                \
    uint  i  = uint(sample_1d * float(distr_len - 1));                  \
    float a  = sample_1d * float(distr_len - 1) - float(i);             \
    if (a == 0.f) {                                                     \
      return distr.func[i];                                             \
    } else {                                                            \
      return mix(distr.func[i], distr.func[i + 1], a);                  \
    }                                                                   \
  }                                                                     \
                                                                        \
  DistributionSampleDiscrete sample_##name##_discrete(in float u) {     \
    DistributionSampleDiscrete ds;                                      \
    ds.i   = sample_##name##_base(u);                                   \
    ds.pdf = distr.func[ds.i];                                          \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  DistributionSampleContinuous sample_##name##_continuous(in float u) { \
    DistributionSampleContinuous ds;                                    \
    uint  i = sample_##name##_base(u);                                  \
    float d = distr.cdf[i + 1] - distr.cdf[i];                          \
    float a = (u - distr.cdf[i]) / d;                                   \
    ds.f = (d == 0.f ? float(i) : float(i) + a) / float(distr_len);     \
    ds.pdf = pdf_##name(ds.f);                                          \
    return ds;                                                          \
  }

#define declare_distr_array_sampler(name, distrs, distr_len)                   \
  uint sample_##name##_base(in uint i, in float u) {                           \
    int j = 0;                                                                 \
    while (u > distrs[i].cdf[j] && j < distr_len)                              \
      j++;                                                                     \
    j -= 1;                                                                    \
    if (distrs[i].func[j] == 0.f && j < distr_len - 1)                         \
      j++;                                                                     \
    return uint(clamp(j, 0, distr_len - 1));                                   \
  }                                                                            \
                                                                               \
  float pdf_##name##_discrete(in uint i, in uint j) {                          \
    return distrs[i].func[j] / float(distr_len);                               \
  }                                                                            \
                                                                               \
  DistributionSampleDiscrete sample_##name##_discrete(in uint i, in float u) { \
    DistributionSampleDiscrete ds;                                             \
    ds.i   = sample_##name##_base(i, u);                                       \
    ds.pdf = pdf_##name##_discrete(i, ds.i);                                   \
    return ds;                                                                 \
  }


#define declare_distr_sampler_default(name)                             \
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
    ds.f   = u;                                                         \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }

#define declare_distr_array_sampler_default(name)    \
  float pdf_##name##_discrete(in uint i, in uint j) {                              \
    return 1.f;                                                         \
  }                                                                     \
                                                                        \
  float pdf_##name(in uint i, in float sample_1d) {                                \
    return 1.f;                                                         \
  }                                                                     \
                                                                        \
  DistributionSampleDiscrete sample_##name##_discrete(in uint i, in float u) {     \
    DistributionSampleDiscrete ds;                                      \
    ds.i   = 0u;                                                        \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  DistributionSampleContinuous sample_##name##_continuous(in uint i, in float u) { \
    DistributionSampleContinuous ds;                                    \
    ds.f   = u;                                                         \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }
#endif // DISTRIBUTION_GLSL_GUARD