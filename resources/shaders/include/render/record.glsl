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

// Getters to read packed Ray/SurfaceInfo data;
// uint records are used by intersection tests to store type and index of a hit
bool record_is_valid(in uint rc)             { return rc != RECORD_INVALID_DATA;       }
bool record_is_emitter(in uint rc)           { return (rc & RECORD_EMITTER_FLAG) != 0; }
bool record_is_object(in uint rc)            { return (rc & RECORD_EMITTER_FLAG) == 0; }
uint record_get_object(in uint rc)           { return (rc >> 24) & 0x0000007F;         }
uint record_get_emitter(in uint rc)          { return (rc >> 24) & 0x0000007F;         }
uint record_get_object_primitive(in uint rc) { return rc & 0x00FFFFFF;                 }
bool record_get_anyhit(in uint rc)           { return rc == 0x1;                       }

// Getters to read packed Object material data;
// uvec2/uint records are used by objects to store material values/texture indices
bool  record_is_sampled(in uvec2 rc) { return (rc.y & 0xFFFF0000) != 0; }
bool  record_is_sampled(in uint rc)  { return (rc & 0xFFFF0000) != 0;   }
bool  record_is_direct(in uvec2 rc)  { return (rc.y & 0xFFFF0000) == 0; }
bool  record_is_direct(in uint rc)   { return (rc & 0xFFFF0000) == 0;   }
uint  record_get_sampler_index(in uvec2 rc) { return rc.x & 0x0000FFFF; }
uint  record_get_sampler_index(in uint rc)  { return rc & 0x0000FFFF;   }
vec3  record_get_direct_value(in uvec2 rc)  { return vec3(unpackHalf2x16(rc.x), 
                                                          unpackHalf2x16(rc.y).x); }
float record_get_direct_value(in uint rc)   { return unpackHalf2x16(rc).x;         }

// Getters to read packed Normalmap material data;
// uint record is used by objects to optional store index data
bool record_is_optional_set(in uint rc)    { return (rc & 0x10000000) != 0; }
uint record_get_optional_value(in uint rc) { return (rc & 0x0FFFFFFF);      }

#endif // RECORD_GLSL_GUARD