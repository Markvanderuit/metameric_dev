#include <preamble.glsl>
#include <math.glsl>

// Fragment input declarations
layout(location = 0) in vec2 in_value_vert;
layout(location = 1) in vec4 in_value_colr;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

float f_gauss(float x, float x0, float sx) {
  float arg = x - x0;
  arg = -1.f /2.f * arg * arg / sx;
  float a = 1.f / (pow(2.f * 3.1415f * sx, 0.5f));
  return a * exp(arg);
}

void main() {
  /* if (sdot(in_value_vert) > 1)
    discard; */
  
  // Simple linear function
  float dist2 = length(in_value_vert);
  float gauss = f_gauss(dist2, 0.f, .01f);
  // float alpha = max(1.f - dist2, 0);

  out_value_colr = vec4(in_value_colr.xyz, in_value_colr.a * gauss);
}