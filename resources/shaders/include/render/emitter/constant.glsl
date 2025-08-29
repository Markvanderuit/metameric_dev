#ifndef RENDER_EMITTER_CONSTANT_GLSL_GUARD
#define RENDER_EMITTER_CONSTANT_GLSL_GUARD

// struct EnvironmentSample {
//   vec4  L;
//   vec3  d;
//   float pdf;
// };

// EnvironmentSample sample_environment_map(in vec2 sample_2d) {
//   uvec2 environment_dims; /* = ... */

//   // Take a sample from the alias table
//   uint i     = uint(sample_2d.x * float(hprod(environment_dims)));
//   uint table = 0; /* texelFetch(b_environment_alias_table, int(i)).x; */
//   uint alias = table % hprod(environment_dims);
//   float prob = float(table / hprod(environment_dims)) 
//              / (0xffffffffu / hprod(environment_dims) - 1);

//   // Select the taken sample, or take its alias
//   i = sample_2d.y < prob ? i : alias;

//   // Turn the sample index into (a) a pixel and (b) coordinates for a direction
//   uvec2 px                 = uvec2(i % environment_dims.x, i / environment_dims.x);
//   float inclination_factor = M_PI / float(environment_dims.y);
//   float inclination        = fma(px.y, inclination_factor, 0.5 * inclination_factor);
//   float azimuth_factor     = 2.f * M_PI / float(environment_dims.x);
//   float azimuth            = fma(px.x, azimuth_factor, .5f * azimuth_factor);

//   // Setup return value
//   EnvironmentSample es;
//   es.d   = vec3(cos(azimuth) * sin(inclination), 
//                 sin(azimuth) * sin(inclination), 
//                 cos(inclination));
//   es.L   = vec4(0); /* texelFetch(b_environment, ivec2(px), 0).xyz; */
//   // es.pdf = /* might wanna deal with this properly */
//   return es;
// }

vec4 eval_emitter_constant(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  // Sample either uplifted texture data, or a specified illuminant
  vec4 L = emitter_spec_type(em) == EmitterSpectrumTypeColor
         ? texture_illuminant(si, wvls, sample_2d)
         : scene_illuminant(em.illuminant_i, wvls);
  return L * em.illuminant_scale;
}

float pdf_emitter_constant(in Emitter em, in Interaction si) {
  return square_to_unif_hemisphere_pdf(si.wi);
}

EmitterSample sample_emitter_constant(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  
  es.is_delta = false;
  es.ray      = ray_towards_direction(si, to_world(si, square_to_unif_hemisphere(sample_2d)));
  es.pdf      = square_to_unif_hemisphere_pdf(es.ray.d);
  
  return es;
}

#endif // RENDER_EMITTER_CONSTANT_GLSL_GUARD