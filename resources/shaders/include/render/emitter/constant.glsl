#ifndef RENDER_EMITTER_CONSTANT_GLSL_GUARD
#define RENDER_EMITTER_CONSTANT_GLSL_GUARD

vec4 eval_emitter_constant(in Emitter em, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  // if (em.spec_type == EmitterSpectrumTypeColor && record_is_sampled(em.color_data)) {
  //   AtlasInfo image = scene_texture_emitter_coef_info(record_get_emitter(si.data));
  //   uvec2 px = uvec2(si.tx * image.size /* - .5f */);
  //   uint  i  = px.y * image.size.x + px.x;
  //   return vec4(pdf_envm_alias_table_discrete(i));
  // }

  // Sample either uplifted texture data, or a specified illuminant
  vec4 L = em.spec_type == EmitterSpectrumTypeColor
         ? texture_illuminant(record_get_emitter(si.data), si.tx, wvls, sample_2d)
         : scene_illuminant(em.illuminant_i, wvls);
  return L * em.illuminant_scale;
}

float pdf_envm_alias_table(in uvec2 px, in uvec2 size) {
  uint i = px.y * size.x + px.x;
  return pdf_envm_alias_table_discrete(i);
}

float pdf_envm_alias_table(in vec2 px, in uvec2 size) {
  vec2 a = fract(px);
  return mix(
    mix(pdf_envm_alias_table(uvec2(px + tx_offsets[0]), size), pdf_envm_alias_table(uvec2(px + tx_offsets[1]), size), a.x),
    mix(pdf_envm_alias_table(uvec2(px + tx_offsets[2]), size), pdf_envm_alias_table(uvec2(px + tx_offsets[3]), size), a.x),
    a.y
  );
}

float pdf_emitter_constant(in Emitter em, in Interaction si) {
  // if (em.spec_type == EmitterSpectrumTypeColor && record_is_sampled(em.color_data)) {
  //   // We have an uplifted image as environment map. Use alias table for importance sampling
  //   AtlasInfo image     = scene_texture_emitter_coef_info(record_get_emitter(si.data));
  //   vec2 px             = si.tx * image.size - .5f;
  //   float inv_sin_theta = sqrt(sdot(si.wi.x) + sdot(si.wi.z));
    
  //   return pdf_envm_alias_table(px, image.size) / inv_sin_theta /* * M_PI_INV / (2.f * M_PI) */;

  //   // uvec2 px = uvec2(si.tx * image.size - .5f);
  //   // uint   i  = px.y * image.size.x + px.x;
  //   // return pdf_envm_alias_table_discrete(i);
  // } else 
  {
    // Envmap is constant in all directions, sampling uniformly for now
    return square_to_unif_hemisphere_pdf(si.wi);
  }
}

EmitterSample sample_emitter_constant(in Emitter em, in uint emitter_i, in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  EmitterSample es;
  es.is_delta = false;

  // if (em.spec_type == EmitterSpectrumTypeColor && record_is_sampled(em.color_data)) {
  //   // We have an uplifted image as environment map. Use alias table to select a pixel
  //   DiscreteSample ds = sample_envm_alias_table_discrete(sample_2d.x);
  //   AtlasInfo   image = scene_texture_emitter_coef_info(emitter_i);

  //   // Compute 2d coordinates in the pixel's center
  //   uvec2 px  = uvec2(ds.i % image.size.x, ds.i / image.size.x);
  //   vec2  tx  = (vec2(px) + .5f) / float(image.size);

  //   // Turn 2d coordinates into direction vector
  //   vec3 d = square_to_unif_sphere(tx).yzx * vec3(1, 1, -1);

  //   // If the direction lies in the upper hemisphere, treat as valid sample for now
  //   if (sample_2d.y >= (cos_theta(to_local(si, d)) * 2.f - 1.f)) {
  //     if (cos_theta(to_local(si, d)) >= 0.f) {
  //       es.is_delta = false;
  //       es.ray      = ray_towards_direction(si, d);

  //       // Override pdf for now
  //       Ray ray = es.ray;
  //       record_set_emitter(ray.data, emitter_i);
  //       es.pdf = pdf_emitter_constant(em, get_interaction(ray)); /* ds.pdf; */ // mis_balance(ds.pdf, square_to_unif_hemisphere_pdf(d));
    
  //     } else {
  //       es = emitter_sample_zero();
  //     }
  //   } else {
  //     d = to_world(si, square_to_unif_hemisphere(sample_2d));
  //     es.ray = ray_towards_direction(si, d);
  //     es.pdf = square_to_unif_hemisphere_pdf(d);
  //   }

  //   // float inv_sin_theta = inversesqrt(sdot(d.x) + sdot(d.z));

  //   /* // Flip samples in negative hemisphere
  //   d = to_local(si, d);
  //   if (cos_theta(d) < 0.f)
  //     d.z = -d.z;
  //   d = to_world(si, d); */
    
  //   /* 
  //     es.pdf = 0.f;
  //   } */
  //   // if (cos_theta(to_local(si, d)) < 0.f) {
  //     // d = to_world(si, square_to_unif_hemisphere(sample_2d));
  //     // es.ray = ray_towards_direction(si, d);
  //     // es.pdf = square_to_unif_hemisphere_pdf(d);
  //     // es = emitter_sample_zero();

  //   //   /* // Fall back to hemisphere sample
      
  //   //   vec2  tx  = mod(vec2(atan(d.x, -d.z) * .5f, acos(d.y)) * M_PI_INV + 1.f, 1.f);
  //   //   uvec2 px  = uvec2(si.tx * image.size - .5f);
  //   //   uint  i   = px.y * image.size.x + px.x;
  //   //   float pdf = pdf_envm_alias_table_discrete(i);

  //   //   es.is_delta = false;
  //   //   es.ray      = ray_towards_direction(si, to_world(si, d));
  //   //   es.pdf      = mis_balance(square_to_unif_hemisphere_pdf(d), pdf); */
  //   // }
  // } else 
  {
    // Envmap is constant in all directions, sampling uniformly for now
    vec3 d      = square_to_unif_hemisphere(sample_2d);
    es.is_delta = false;
    es.ray      = ray_towards_direction(si, to_world(si, d));
    es.pdf      = square_to_unif_hemisphere_pdf(d);
  }

  return es;
}

#endif // RENDER_EMITTER_CONSTANT_GLSL_GUARD