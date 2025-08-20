#ifndef DISTRIBUTION_GLSL_GUARD
#define DISTRIBUTION_GLSL_GUARD

// Return values for sample functions
struct DiscreteSample {
  uint  i;
  float pdf;
};
struct ContinuousSample {
  float f;
  float pdf;
};
struct AliasTableBin {
  float p;
  float q;
  int   alias;
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
  DiscreteSample sample_##name##_discrete(in float u) {                 \
    DiscreteSample ds;                                                  \
    ds.i   = sample_##name##_base(u);                                   \
    ds.pdf = distr.func[ds.i];                                          \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  ContinuousSample sample_##name##_continuous(in float u) {             \
    ContinuousSample ds;                                                \
    uint  i = sample_##name##_base(u);                                  \
    float d = distr.cdf[i + 1] - distr.cdf[i];                          \
    float a = (u - distr.cdf[i]) / d;                                   \
    ds.f = (d == 0.f ? float(i) : float(i) + a) / float(distr_len);     \
    ds.pdf = pdf_##name(ds.f);                                          \
    return ds;                                                          \
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
  DiscreteSample sample_##name##_discrete(in float u) {                 \
    DiscreteSample ds;                                                  \
    ds.i   = 0u;                                                        \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  ContinuousSample sample_##name##_continuous(in float u) {             \
    ContinuousSample ds;                                                \
    ds.f   = u;                                                         \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }

#define declare_alias_sampler(name, bins, bins_len)                     \
  DiscreteSample sample_##name##(in float u) {                          \
    int i = min(u * bins_len, bins_len - 1);                            \
    float up = min(float(u * bins_len - i), 0x1.fffffep-1);             \
    i = (up < bins[i].q) ? i : bins[i].alias;                           \
    DiscreteSample ds;                                                  \
    ds.i   = i;                                                         \
    ds.pdf = bins[i].p;                                                 \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  float pdf_##name##(in uint i){                                        \
    return bins[i].p;                                                   \
  }

  #define declare_alias_sampler_default(name)                           \
  DiscreteSample sample_##name##(in float u) {                          \
    DiscreteSample ds;                                                  \
    ds.i   = 0;                                                         \
    ds.pdf = 1.f;                                                       \
    return ds;                                                          \
  }                                                                     \
                                                                        \
  float pdf_##name##(in uint i){                                        \
    return 1.f;                                                         \
  }

#endif // DISTRIBUTION_GLSL_GUARD