#ifndef SPECTRUM_SUBGROUP_GLSL_GUARD
#define SPECTRUM_SUBGROUP_GLSL_GUARD

// Enable subgroup extensions
#extension GL_KHR_shader_subgroup_basic      : require
#extension GL_KHR_shader_subgroup_arithmetic : require

// Define metameric's spectral range layout
const float wavelength_min     = MET_WAVELENGTH_MIN;
const float wavelength_max     = MET_WAVELENGTH_MAX;
const uint  wavelength_samples = MET_WAVELENGTH_SAMPLES;

// Define derived variables from metameric's spectral range layout
const float wavelength_range = wavelength_max - wavelength_min;
const float wavelength_ssize = wavelength_range / float(wavelength_samples);
const float wavelength_ssinv = float(wavelength_samples) / wavelength_range;

// Define derived variables for subgroup operations
const uint subgroup_samples = gl_SubgroupSize;

/* Base Spectrum object */

#define Spec float;
#define CMFS float[3];


#endif // SPECTRUM_SUBGROUP_GLSL_GUARD