#ifndef SPECTRUM_CONSTANTS_GLSL_GUARD
#define SPECTRUM_CONSTANTS_GLSL_GUARD

// Define metameric's spectral layout
const float wavelength_min     = MET_WAVELENGTH_MIN;
const float wavelength_max     = MET_WAVELENGTH_MAX;
const uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;
const uint  wavelength_bases   = MET_WAVELENGTH_BASES;

// Define derived variables from metameric's spectral layout
const float wavelength_range       = wavelength_max - wavelength_min;
const float wavelength_ssize       = wavelength_range / float(wavelength_samples);
const float wavelength_ssinv       = float(wavelength_samples) / wavelength_range;
const float wavelength_samples_inv = 1.f / float(wavelength_samples);

// Define derived variables for better aligned operations
const uint wavelength_samples_al = uint(pow(2, ceil(log2(float(wavelength_samples)))));

// Take a sample in [0, 1] and transform to e.g. [400, 700]
float sample_to_wavelength(float x) {
  return wavelength_min + wavelength_range * x;
}

// Wavelengths in the renderer are actually [0, 1], not e.g. [400, 700]
uint index_at_wavelength(float wvl) {
  return min(uint(wvl * wavelength_samples), wavelength_samples - 1);
}

#endif // SPECTRUM_CONSTANTS_GLSL_GUARD
