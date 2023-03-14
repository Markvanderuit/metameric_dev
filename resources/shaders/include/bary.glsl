#ifndef BARY_GLSL_GUARD
#define BARY_GLSL_GUARD

const uint mvc_weights   = MET_MVC_WEIGHTS;
const uint mvc_weights_4 = MET_MVC_WEIGHTS / 4;

#define Bary  float[mvc_weights]
#define Bary4 vec4[mvc_weights_4]
#define BaryP Bary4

Bary4 unpack(in BaryP w) {
  return w;
}

BaryP pack(in Bary4 w) {
  return w;
}

vec4 unpack(in BaryP w, uint i) {
  return w[i];
}

/* #define BaryP uvec4[mvc_weights / 8]

vec4 unpack(in uvec2 f) {
  return vec4(unpackHalf2x16(f.x), unpackHalf2x16(f.y));
}

uvec2 pack(in vec4 f) {
  return uvec2(packHalf2x16(f.xy), packHalf2x16(f.zw));
}

vec4 unpack(in BaryP iw, uint i) {
  return i % 2 == 0
         ? unpack(iw[i / 2].xy)
         : unpack(iw[i / 2].zw);
}

Bary4 unpack(in BaryP iw) {
  Bary4 ow;
  for (uint i = 0; i < mvc_weights / 8; ++i) {
    ow[2 * i    ] = unpack(iw[i].xy);
    ow[2 * i + 1] = unpack(iw[i].zw);
  }
  return ow;
}

BaryP pack(in Bary4 iw) {
  BaryP ow;
  for (uint i = 0; i < mvc_weights / 8; ++i) {
    ow[i].xy = pack(iw[2 * i    ]);
    ow[i].zw = pack(iw[2 * i + 1]);
  }
  return ow;
} */

#endif // BARY_GLSL_GUARD