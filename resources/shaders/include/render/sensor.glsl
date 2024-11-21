#ifndef RENDER_SENSOR_GLSL_GUARD
#define RENDER_SENSOR_GLSL_GUARD

#include <render/ray.glsl>

// Simple sensor definition based on matrices
struct FilmSensor {
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size;
};

// Simple sensor definition based on matrices,
// for a specific pixel
struct PixelSensor {
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size;
  uvec2 pixel;
};

// Simple sensor definition, for a single ray
struct RaySensor {
  vec3 o, d;
};

// Sample from sensor at a specific pixel
struct SensorSample {
  Ray  ray;  // Sensor ray
  vec4 wvls; // Packet of wavelengths along ray
  vec4 pdfs; // Packet of pdfs for each ray/wavelength pair
};

// Simple hero sampling helper
float rotate_sample_1d(in float sample_1d, in uint i, in uint n) {
  return mod(sample_1d + float(i) / float(n), 1.f);
}

// Simple box filter
vec2 sample_box_filter(in vec2 sample_2d) {
  return sample_2d - 0.5; // [-.5, .5]
}

// Simple tent filter
vec2 sample_tent_filter(in vec2 sample_2d) {
  vec2 xy = 2 * sample_2d;
  xy = mix(sqrt(xy) - 1.f, 
           1.f - sqrt(2.f - xy), 
           lessThan(xy, vec2(1)));
  return xy; // [-1, 1]
}

// Given a ray sensor, generate a sample
SensorSample sample_sensor(in RaySensor sensor, in float sample_1d) {
  SensorSample ss;

  // Generate camera ray from sample
  ss.ray = init_ray(sensor.o, sensor.d);

  // Sample wavelengths; stratified sample through invercse cdf, if available
  for (uint i = 0; i < 4; ++i) {
    DistributionSampleContinuous ds = sample_wavelength_continuous(rotate_sample_1d(sample_1d, i, 4));
    ss.wvls[i] = ds.f;
    ss.pdfs[i] = ds.pdf;
  }

  return ss;
}

// Given a single pixel sensor, generate a sample
SensorSample sample_sensor(in PixelSensor sensor, in vec3 sample_3d) {
  SensorSample ss;

  // Get necessary sensor information
  float tan_y    = 1.f / sensor.proj_trf[1][1];
  float aspect   = float(sensor.film_size.x) / float(sensor.film_size.y);
  mat4  view_inv = inverse(sensor.view_trf);

  // Sample film position inside pixel, transform to [-1, 1]
  vec2 xy = (vec2(sensor.pixel) + 0.5 + sample_tent_filter(sample_3d.xy)) / vec2(sensor.film_size);
  xy = (xy - .5f) * 2.f;
  
  // Generate camera ray from sample
  ss.ray = init_ray(
    (view_inv * vec4(0, 0, 0, 1)).xyz,
    normalize((view_inv * vec4(xy.x * tan_y * aspect, xy.y * tan_y, -1, 0)).xyz)
  );

  // Sample wavelengths; stratified sample through invercse cdf, if available
  for (uint i = 0; i < 4; ++i) {
    DistributionSampleContinuous ds = sample_wavelength_continuous(rotate_sample_1d(sample_3d.z, i, 4));
    ss.wvls[i] = ds.f;
    ss.pdfs[i] = ds.pdf;
  }

  return ss;
}

// Given a full film camera sensor, generate a sample
SensorSample sample_sensor(in FilmSensor sensor, in ivec2 px, in uint sample_i, in vec3 sample_3d) {
  SensorSample ss;

  // Stratify into n_bins^2 subpixels
  const uint n_bins = 2;
  vec2 sub_xy = vec2(sample_i % n_bins, (sample_i / n_bins) % n_bins); // ([0, 0], [1, 0], [0, 1], [1, 1])

  // Sample film position inside subpixel, transform to [-1, 1]
  vec2 xy = (vec2(px) 
          + (sub_xy + 0.5 + sample_tent_filter(sample_3d.xy)) / vec2(n_bins)) 
          / vec2(sensor.film_size);
  xy = (xy - .5f) * 2.f;
  
  // Get necessary sensor information
  float tan_y    = 1.f / sensor.proj_trf[1][1];
  float aspect   = float(sensor.film_size.x) / float(sensor.film_size.y);
  mat4  view_inv = inverse(sensor.view_trf);

  // Generate camera ray from sample
  ss.ray = init_ray(
    (view_inv * vec4(0, 0, 0, 1)).xyz,
    normalize((view_inv * vec4(xy.x * tan_y * aspect, xy.y * tan_y, -1, 0)).xyz)
  );

  // Sample wavelengths; stratified sample through invercse cdf, if available
  for (uint i = 0; i < 4; ++i) {
    DistributionSampleContinuous ds = sample_wavelength_continuous(rotate_sample_1d(sample_3d.z, i, 4));
    ss.wvls[i] = ds.f;
    ss.pdfs[i] = ds.pdf;
  }

  return ss;
}

vec3 sensor_apply(in vec4 wvls, in vec4 L) {
  return (scene_cmfs(0, wvls) * L) * 0.25f * float(wavelength_samples);
}

vec3 sensor_apply(in SensorSample sensor_sample, in vec4 L) {
  return (scene_cmfs(0, sensor_sample.wvls) * L) * 0.25f * float(wavelength_samples);
}

#endif // RENDER_SENSOR_GLSL_GUARD