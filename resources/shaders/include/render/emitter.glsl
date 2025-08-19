#ifndef RENDER_EMITTER_GLSL_GUARD
#define RENDER_EMITTER_GLSL_GUARD

#include <math.glsl>
#include <distribution.glsl>
#include <render/ray.glsl>
#include <render/frame.glsl>
#include <render/sample.glsl>
#include <render/interaction.glsl>
#include <render/warp.glsl>
#include <render/texture.glsl>
#include <render/shape/sphere.glsl>
#include <render/shape/rectangle.glsl>
#include <render/emitter/sphere.glsl>
#include <render/emitter/rectangle.glsl>
#include <render/emitter/point.glsl>
#include <render/emitter/constant.glsl>

vec4 eval_emitter(in Interaction si, in vec4 wvls, in vec2 sample_2d) {
  if (!is_emitter(si))
    return vec4(0);
  
  Emitter em = scene_emitter_info(record_get_emitter(si.data));
  
  if (em.type == EmitterTypeSphere) {
    return eval_emitter_sphere(em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypeRectangle) {
    return eval_emitter_rectangle(em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypePoint) {
    return eval_emitter_point(em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypeConstant) {
    return eval_emitter_constant(em, si, wvls, sample_2d);
  }
}

vec4 eval_emitter(in EmitterSample es, in vec4 wvls, in vec2 sample_2d) {
  Interaction si = get_interaction(es.ray);
  return eval_emitter(si, wvls, sample_2d);
}

float pdf_emitter(in Interaction si) {
  if (!is_emitter(si))
    return 0.f;

  // Evaluate pdf from emitter
  Emitter em = scene_emitter_info(record_get_emitter(si.data));
  float pdf = pdf_emitters_discrete(record_get_emitter(si.data));
  
  if (em.type == EmitterTypeSphere) {
    pdf *= pdf_emitter_sphere(em, si);
  } else if (em.type == EmitterTypeRectangle) {
    pdf *= pdf_emitter_rectangle(em, si);
  } else if (em.type == EmitterTypePoint) {
    pdf *= pdf_emitter_point(em, si);
  } else if (em.type == EmitterTypeConstant) {
    pdf *= pdf_emitter_constant(em, si);
  }

  return pdf;
}

EmitterSample sample_emitter(in Interaction si, in vec4 wvls, in vec3 sample_3d) {
  // Sample specific emitter from distribution
  DistributionSampleDiscrete ds = sample_emitters_discrete(sample_3d.z);
  Emitter em = scene_emitter_info(ds.i);
  if (!em.is_active)
    return emitter_sample_zero();

  // Sample specific position on emitter
  EmitterSample es;
  if (em.type == EmitterTypeSphere) {
    es = sample_emitter_sphere(em, si, wvls, sample_3d.xy);
  } else if (em.type == EmitterTypeRectangle) {
    es = sample_emitter_rectangle(em, si, wvls, sample_3d.xy);
  } else if (em.type == EmitterTypePoint) {
    es = sample_emitter_point(em, si, wvls, sample_3d.xy);
  } else if (em.type == EmitterTypeConstant) {
    es = sample_emitter_constant(em, si, wvls, sample_3d.xy);
  }

  // Multiply sample pdfs
  es.pdf *= ds.pdf;

  // Store emitter index in ray record
  record_set_emitter(es.ray.data, ds.i);

  return es;
}

bool ray_intersect_emitter(inout Ray ray, in uint emitter_i) {
  Emitter em = scene_emitter_info(emitter_i);
  if (!em.is_active)
    return false;

  // Run intersection; flag result
  bool hit;
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.trf[3].xyz, .5f * length(em.trf[0].xyz) };
    hit = ray_intersect(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    Rectangle rectangle = { em.trf[3].xyz, em.trf[0].xyz, em.trf[1].xyz, normalize(em.trf[2].xyz) };
    hit = ray_intersect(ray, rectangle);
  } else {
    hit = false;
  }

  // Store emitter index in ray record on closest hit
  if (hit)
    record_set_emitter(ray.data, emitter_i);
  
  return hit;
}

bool ray_intersect_emitter_any(in Ray ray, in uint emitter_i) {
  Emitter em = scene_emitter_info(emitter_i);
  if (!em.is_active)
    return false;
  
  // Run intersection; on a hit, simply return
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.trf[3].xyz, .5f * length(em.trf[0].xyz) };
    return ray_intersect_any(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    Rectangle rectangle = { em.trf[3].xyz, em.trf[0].xyz, em.trf[1].xyz, normalize(em.trf[2].xyz) };
    return ray_intersect_any(ray, rectangle);
  } else {
    return false;
  }
}

#endif // RENDER_EMITTER_GLSL_GUARD