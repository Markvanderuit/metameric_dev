#ifndef EMITTER_GLSL_GUARD
#define EMITTER_GLSL_GUARD

#include <math.glsl>
#include <ray.glsl>
#include <frame.glsl>
#include <warp.glsl>
#include <scene.glsl>
#include <emitter/sphere.glsl>
#include <emitter/rect.glsl>
#include <emitter/point.glsl>
#include <emitter/constant.glsl>

PositionSample sample_emitter(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  if (em.type == EmitterTypeSphere) {
    return sample_emitter_sphere(em, si, sample_2d);
  } else if (em.type == EmitterTypeRect) {
    return sample_emitter_rect(em, si, sample_2d);
  } else if (em.type == EmitterTypePoint) {
    return sample_emitter_point(em, si, sample_2d);
  } else if (em.type == EmitterTypeConstant) {
    return sample_emitter_constant(em, si, sample_2d);
  }
}

PositionSample sample_emitter(in SurfaceInfo si, in uint emitter_i, in vec2 sample_2d) {
  return sample_emitter(scene_emitter_info(emitter_i), si, sample_2d);
}

vec4 eval_emitter(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  if (em.type == EmitterTypeSphere) {
    return eval_emitter_sphere(em, ps, wvls);
  } else if (em.type == EmitterTypeRect) {
    return eval_emitter_rect(em, ps, wvls);
  } else if (em.type == EmitterTypePoint) {
    return eval_emitter_point(em, ps, wvls);
  } else if (em.type == EmitterTypeConstant) {
    return eval_emitter_constant(em, ps, wvls);
  }
}

vec4 eval_emitter(in PositionSample ps, in vec4 wvls) {
  if (!record_is_emitter(ps.data))
    return vec4(0);
  return eval_emitter(scene_emitter_info(record_get_emitter(ps.data)), ps, wvls);
}

float pdf_emitter(in EmitterInfo em, in PositionSample ps) {
  if (em.type == EmitterTypeSphere) {
    return pdf_emitter_sphere(em, ps);
  } else if (em.type == EmitterTypeRect) {
    return pdf_emitter_rect(em, ps);
  } else if (em.type == EmitterTypePoint) {
    return pdf_emitter_point(em, ps);
  } else {
    return pdf_emitter_constant(em, ps);
  }
}

float pdf_emitter(in PositionSample ps) {
  if (!record_is_emitter(ps.data))
    return 0.f;
  return pdf_emitter(scene_emitter_info(record_get_emitter(ps.data)), ps);
}

PositionSample sample_emitters(in SurfaceInfo si, in vec3 sample_3d) {
  // TODO pick emitter from distribution
  float emitter_sample = sample_3d.z;
  uint  emitter_i      = 0;
  float emitter_pdf    = 1.f;
  
  PositionSample ps = sample_emitter(s_emtr_info[emitter_i], si, sample_3d.xy);
  record_set_emitter(ps.data, emitter_i);
  ps.pdf *= emitter_pdf;
  return ps;
}

float pdf_emitters(in PositionSample ps) {
  // TODO Multiply against emitter pdf from distribution
  return pdf_emitter(ps) * 1.f;
}

#endif // EMITTER_GLSL_GUARD