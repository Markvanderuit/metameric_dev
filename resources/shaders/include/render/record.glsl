#ifndef RECORD_GLSL_GUARD
#define RECORD_GLSL_GUARD

// Flag value to encode object/emitter hit data in a single uint
#define RECORD_INVALID_DATA 0xFFFFFFFF
#define RECORD_OBJECT_FLAG  0x00000000
#define RECORD_EMITTER_FLAG 0x80000000

// Clear object/emitter record data
void record_clear(inout uint rc) {
  rc = RECORD_INVALID_DATA;
}

// Specify an object in record data
void record_set_object(inout uint rc, in uint object_i) {
  rc = RECORD_OBJECT_FLAG
     | ((object_i & 0x7F) << 24)
     | (rc        & 0x00FFFFFF);
}

// Specify an emitter in record data
void record_set_emitter(inout uint rc, in uint emitter_i) {
  rc = RECORD_EMITTER_FLAG
     | ((emitter_i & 0x7F) << 24)
     | (rc         & 0x00FFFFFF);
}

// Specify a primitive, given object data is present
void record_set_object_primitive(inout uint rc, in uint primitive_i) {
  rc = RECORD_OBJECT_FLAG
     | (rc          & 0x7F000000)
     | (primitive_i & 0x00FFFFFF);
}

// Specify a record as storing a boolean
void record_set_anyhit(inout uint rc, in bool hit) { 
  rc = uint(hit); 
}

// Getters to read stored ray hit record data;
// uint records are used by intersection tests to store type and index of a hit
bool record_is_valid(in uint rc)             { return rc != RECORD_INVALID_DATA;      }
bool record_is_emitter(in uint rc)           { return (rc & RECORD_EMITTER_FLAG) != 0;}
bool record_is_object(in uint rc)            { return (rc & RECORD_EMITTER_FLAG) == 0;}
uint record_get_object(in uint rc)           { return (rc >> 24) & 0x0000007F;        }
uint record_get_emitter(in uint rc)          { return (rc >> 24) & 0x0000007F;        }
uint record_get_object_primitive(in uint rc) { return rc & 0x00FFFFFF;                }
bool record_get_anyhit(in uint rc)           { return rc == 0x1;                      }

// Getters to read stored material record data;
// uvec4/vec2 records are used by objects to store material values/texture indices
bool  record_is_sampled(in uvec4 rc)        { return rc.x != 0;               }
bool  record_is_sampled(in uvec2 rc)        { return rc.x != 0;               }
bool  record_is_direct(in uvec4 rc)         { return rc.x == 0;               }
bool  record_is_direct(in uvec2 rc)         { return rc.x == 0;               }
uint  record_get_sampler_index(in uvec4 rc) { return rc.y;                    }
uint  record_get_sampler_index(in uvec2 rc) { return rc.y;                    }
vec3  record_get_direct_value(in uvec4 rc)  { return uintBitsToFloat(rc.yzw); }
float record_get_direct_value(in uvec2 rc)  { return uintBitsToFloat(rc.y);   }

struct PositionSample {
  // Position/normal on surface of entity
  vec3  p, n;

  // Exitant sampled direction to p, world space
  vec3  d;

  // Distance from surface to position
  float t;

  // Sampling density
  float pdf;

  // Is the sample some weird dirac
  bool is_delta;

  // Record referring to underlying entity
  uint data;
};

struct BRDFSample {
  // Is the sample some weird dirac
  bool is_delta;

  // Exitant sampled direction, local space
  vec3 wo;
  
  // Sampling density
  float pdf;
};

struct MicrofacetSample {
  // Microfacet surface normal, sampled from a probability density
  vec3 n;

  // Sampling density
  float pdf;
};

// Create an invalid sample
PositionSample position_sample_zero() {
  PositionSample ps;
  ps.is_delta = false;
  ps.pdf      = 0.f;
  return ps;
}

// Create an invalid sample
BRDFSample brdf_sample_zero() {
  BRDFSample bs;
  bs.is_delta = false;
  bs.pdf      = 0.f;
  return bs;
}

// Create an invalid sample
MicrofacetSample microfacet_sample_zero() {
  MicrofacetSample ms;
  ms.pdf = 0.f;
  return ms;
}

#endif RECORD_GLSL_GUARD