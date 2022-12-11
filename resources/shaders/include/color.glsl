#ifndef COLOR_GLSL_GUARD
#define COLOR_GLSL_GUARD

float lrgb_to_srgb(in float f) {
  return f <= 0.003130f
       ? f * 12.92
       : pow(f, 1.f / 2.4f) * 1.055f - 0.055f;
}

vec3 lrgb_to_srgb(in vec3 c) {
  return mix(pow(c, vec3(1.f / 2.4f)) * 1.055f - 0.055f,
             c * 12.92f,
             lessThanEqual(c, vec3(0.003130f)));
}

vec4 lrgb_to_srgb(in vec4 c) {
  return vec4(lrgb_to_srgb(c.xyz), c.w);
}

float srgb_to_lrgb(in float f) {
  return f <= 0.04045f
       ? f / 12.92f
       : pow((f + 0.055f) / 1.055f, 2.4f);
}

vec3 srgb_to_lrgb(in vec3 c) {
  return mix(pow((c + 0.055f) / 1.055f, vec3(2.4f)),
             c / 12.92f,
             lessThanEqual(c, vec3(0.04045f)));
}

vec4 srgb_to_lrgb(in vec4 c) {
  return vec4(srgb_to_lrgb(c.xyz), c.w);
}

#endif // COLOR_GLSL_GUARD