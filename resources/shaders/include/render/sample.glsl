#ifndef SAMPLE_GLSL_GUARD
#define SAMPLE_GLSL_GUARD

#include <render/ray.glsl>

// Positional sample on some scene emitter
struct EmitterSample {
  // Ray towards sample position on surface of emitter
  Ray ray;
  
  // Is the sample a dirac delta
  bool is_delta;

  // Emitted radiance along ray towards position
  vec4 L;
  
  // Sample density
  float pdf;
};

struct BRDFSample {
  // Is the sample a dirac delta
  bool is_delta;

  // Is the sample wavelength-dependent (and thus collapsing to a single wvl for now)
  bool is_spectral;

  // Exitant sampled direction, local space
  vec3 wo;
  
  // Sampling density
  float pdf;
};

struct MicrofacetSample {
  // Microfacet sampled surface normal
  vec3 n;

  // Sampling density
  float pdf;
};

EmitterSample emitter_sample_zero() {
  EmitterSample ps;
  ps.is_delta = false;
  ps.pdf      = 0.f;
  return ps;
}

// Create an invalid sample
BRDFSample brdf_sample_zero() {
  BRDFSample bs;
  bs.is_delta    = false;
  bs.is_spectral = false;
  bs.pdf         = 0.f;
  return bs;
}

// Create an invalid sample
MicrofacetSample microfacet_sample_zero() {
  MicrofacetSample ms;
  ms.pdf = 0.f;
  return ms;
}

#endif // SAMPLE_GLSL_GUARD