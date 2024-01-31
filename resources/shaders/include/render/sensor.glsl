#ifndef RENDER_SENSOR_GLSL_GUARD
#define RENDER_SENSOR_GLSL_GUARD

// Simple sensor definition based on matrices
struct Sensor {
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size;
};

struct RaySensor {
  vec3 o, d;
};

// Sample from sensor at a specific pixel
struct SensorSample {
  Ray  ray;  // Sensor ray
  vec4 wvls; // Packet of wavelengths along ray
  vec4 pdfs; // Packet of pdfs for each ray/wavelength pair
};

float rotate_sample_1d(in float sample_1d, in uint i, in uint n) {
  return mod(sample_1d + float(i) / float(n), 1.f);
}

SensorSample sample_sensor(in RaySensor sensor, in float sample_1d) {
  SensorSample ss;

  // Generate camera ray from sample
  ss.ray = init_ray(sensor.o, sensor.d);

  // Sample wavelengths; stratified sample through invercse cdf, if available
  for (uint i = 0; i < 4; ++i) {
    #ifdef SCENE_DATA_AVAILABLE
    DistributionSampleContinuous ds = sample_wavelength_continuous(rotate_sample_1d(sample_1d, i, 4));
    ss.wvls[i] = ds.f;
    ss.pdfs[i] = ds.pdf;
    #else // SCENE_DATA_AVAILABLE
    ss.wvls[i] = rotate_sample_1d(sample_1d);
    ss.pdfs[i] = 1.f;
    #endif // SCENE_DATA_AVAILABLE
  }

  return ss;
}

SensorSample sample_sensor(in Sensor sensor, in ivec2 px, in vec3 sample_3d) {
  SensorSample ss;

  // Get necessary sensor information
  float tan_y    = 1.f / sensor.proj_trf[1][1];
  float aspect   = float(sensor.film_size.x) / float(sensor.film_size.y);
  mat4  view_inv = inverse(sensor.view_trf);

  // Sample film position inside pixel, transform to [-1, 1]
  vec2 xy = (vec2(px) + sample_3d.xy)  / vec2(sensor.film_size);
  xy = (xy - .5f) * 2.f;
  
  // Generate camera ray from sample
  ss.ray = init_ray(
    (view_inv * vec4(0, 0, 0, 1)).xyz,
    normalize((view_inv * vec4(xy.x * tan_y * aspect, xy.y * tan_y, -1, 0)).xyz)
  );

  // Sample wavelengths; stratified sample through invercse cdf, if available
  for (uint i = 0; i < 4; ++i) {
    #ifdef SCENE_DATA_AVAILABLE
    DistributionSampleContinuous ds = sample_wavelength_continuous(rotate_sample_1d(sample_3d.z, i, 4));
    ss.wvls[i] = ds.f;
    ss.pdfs[i] = ds.pdf;
    #else // SCENE_DATA_AVAILABLE
    ss.wvls[i] = rotate_sample_1d(sample_3d.z);
    ss.pdfs[i] = 1.f;
    #endif // SCENE_DATA_AVAILABLE
  }

  return ss;
}

vec3 sensor_apply(in vec4 wvls, in vec4 L) {
  return scene_cmfs(0, wvls) * L;
}

vec3 sensor_apply(in SensorSample sensor_sample, in vec4 L) {
  return scene_cmfs(0, sensor_sample.wvls) * L;
}

#endif // RENDER_SENSOR_GLSL_GUARD