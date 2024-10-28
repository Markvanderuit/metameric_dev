#ifndef RENDER_DETAIL_PACKING_GLSL_GUARD
#define RENDER_DETAIL_PACKING_GLSL_GUARD

#include <spectrum.glsl>

float[8] unpack_snorm_8(in uvec4 p) {
  float[8] m;
  vec2 f2;
  f2 = unpackSnorm2x16(p[0]);
  m[0] = f2.x;
  m[1] = f2.y;
  f2 = unpackSnorm2x16(p[1]);
  m[2] = f2.x;
  m[3] = f2.y;
  f2 = unpackSnorm2x16(p[2]);
  m[4] = f2.x;
  m[5] = f2.y;
  f2 = unpackSnorm2x16(p[3]);
  m[6] = f2.x;
  m[7] = f2.y;
  return m;
}

// Extract a single value instead of unpacking the whole lot
float extract_snorm_8(in uvec4 p, in uint i) {
  return unpackSnorm2x16(p[i / 2])[i % 2];
}

float[12] unpack_snorm_12(in uvec4 p) {
  float[12] m;
  for (int i = 0; i < 12; ++i) {
    int offs = i % 3 == 2 ? 10 : 11; // 11, 11, 10, 11, 11, 10, ...
    uint j = bitfieldExtract(p[i / 3],                // 0,  0,  0,  1,  1,  1,  ...
                             (i % 3) * 11,            // 0,  11, 22, 0,  11, 22, ...
                             offs);
    float scale = float((1 << offs) - 1);
    m[i] = fma(float(j) / scale, 2.f, - 1.f);
  }
  return m;
}

// Extract a single value instead of unpacking the whole lot
float extract_snorm_12(in uvec4 p, in uint i) {
  int offs = i % 3 == 2 ? 10 : 11; // 11, 11, 10, 11, 11, 10, ...
  uint j = bitfieldExtract(p[i / 3],          // 0,  0,  0,  1,  1,  1,  ...
                           (int(i) % 3) * 11, // 0,  11, 22, 0,  11, 22, ...
                           offs);                        
  float scale = float((1 << offs) - 1);
  return fma(float(j) / scale, 2.f, -1.f);
}

uvec4 pack_snorm_8(in float[8] v) {
  uvec4 p;
  p[0] = packSnorm2x16(vec2(v[0], v[1]));
  p[1] = packSnorm2x16(vec2(v[2], v[3]));
  p[2] = packSnorm2x16(vec2(v[4], v[5]));
  p[3] = packSnorm2x16(vec2(v[6], v[7]));
  return p;
}

uvec4 pack_snorm_12(in float[12] v) {
  uvec4 p;
  for (int i = 0; i < 12; ++i) {
    float scale = i % 3 == 2 
                ? float((1 << 10) - 1) 
                : float((1 << 11) - 1);
    uint j = uint(round(clamp((v[i] + 1.f) * .5f, 0.f, 1.f) * scale));
    p[i / 3] = bitfieldInsert(p[i / 3],                // 0,  0,  0,  1,  1,  1,  ...
                              j,
                              (i % 3) * 11,            // 0,  11, 22, 0,  11, 22, ...
                              (i % 3) == 2 ? 10 : 11); // 11, 11, 10, 11, 11, 10, ...
  }
  return p;
}

float[16] unpack_snorm_16(in uvec4 p) {
  float[16] m;
  vec4 f4;
  f4 = unpackSnorm4x8(p[0]);
  m[0] = f4[0];
  m[1] = f4[1];
  m[2] = f4[2];
  m[3] = f4[3];
  f4 = unpackSnorm4x8(p[1]);
  m[4] = f4[0];
  m[5] = f4[1];
  m[6] = f4[2];
  m[7] = f4[3];
  f4 = unpackSnorm4x8(p[2]);
  m[8]  = f4[0];
  m[9] = f4[1];
  m[10] = f4[2];
  m[11] = f4[3];
  f4 = unpackSnorm4x8(p[3]);
  m[12] = f4[0];
  m[13] = f4[1];
  m[14] = f4[2];
  m[15] = f4[3];
  return m;
}

// Extract a single value instead of unpacking the whole lot
float extract_snorm_16(in uvec4 p, in uint i) {
  return unpackSnorm4x8(p[i / 4])[i % 4];
}

uvec4 pack_snorm_16(in float[16] v) {
  uvec4 p;
  p[0] = packSnorm4x8(vec4(v[0],  v[1],  v[2],  v[3]));
  p[1] = packSnorm4x8(vec4(v[4],  v[5],  v[6],  v[7]));
  p[2] = packSnorm4x8(vec4(v[8],  v[9],  v[10], v[11]));
  p[3] = packSnorm4x8(vec4(v[12], v[13], v[14], v[15]));
  return p;
}

float[8] unpack_half_8x16(in uvec4 p) {
  vec2 a = unpackHalf2x16(p[0]), b = unpackHalf2x16(p[1]),
       c = unpackHalf2x16(p[2]), d = unpackHalf2x16(p[3]);
  return float[8](a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1]);
}

uvec4 pack_half_8x16(in float[8] m) {
  return uvec4(packHalf2x16(vec2(m[0], m[1])), packHalf2x16(vec2(m[2], m[3])),
               packHalf2x16(vec2(m[4], m[5])), packHalf2x16(vec2(m[6], m[7])));
}

float[wavelength_bases] unpack_basis_coeffs(in uvec4 p) {
  float[wavelength_bases] v;
#if   MET_WAVELENGTH_BASES == 16
  v = unpack_snorm_16(p);
#elif MET_WAVELENGTH_BASES == 12
  v = unpack_snorm_12(p);
#elif MET_WAVELENGTH_BASES == 8
  v = unpack_snorm_8(p);
#else
  // ...
#endif;
  return v;
}

float extract_basis_coeff(in uvec4 p, in uint i) {
  float f;
#if   MET_WAVELENGTH_BASES == 16
  f = extract_snorm_16(p, i);
#elif MET_WAVELENGTH_BASES == 12
  f = extract_snorm_12(p, i);
#elif MET_WAVELENGTH_BASES == 8
  f = extract_snorm_8(p, i);
#else
  // ...
#endif;
  return f;
}

uvec4 pack_basis_coeffs(in float[wavelength_bases] v) {
  uvec4 p;
#if   MET_WAVELENGTH_BASES == 16
  p = pack_snorm_16(v);
#elif MET_WAVELENGTH_BASES == 12
  p = pack_snorm_12(v);
#elif MET_WAVELENGTH_BASES == 8
  p = pack_snorm_8(v);
#else
  // ...
#endif;
  return p;
}

#endif // RENDER_DETAIL_PACKING_GLSL_GUARD