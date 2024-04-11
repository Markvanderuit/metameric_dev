#include <preamble.glsl>
#include <math.glsl>

// Passthrough vertex-fragment declarations
layout(location = 0)  in  vec2 in_txuv[];
layout(location = 0)  out vec2 out_txuv;

// Geometry stage declaration
layout(triangles)        in;
layout(triangle_strip)   out;
layout(max_vertices = 3) out;

vec2 primitive_vert_expander(in vec2 a, in vec2 b, in vec2 c) {
  float n_ab = distance(a, b);
  float n_ac = distance(a, c);
  float r    = n_ab / (n_ab + n_ac);
  vec2  d    = (b - a) + (c - b) * r;
  return -d;
}

const float expansion_dist = .005f;

void main() {
  for (uint i = 0; i < 3; ++i) {
    uint li = (i + 2) % 3, ri = (i + 1) % 3;

    // Get unnormalized directions away from vertex to expand/shrink triangle
    vec2 d_gl = primitive_vert_expander(gl_in[i].gl_Position.xy,
                                        gl_in[li].gl_Position.xy,
                                        gl_in[ri].gl_Position.xy);
    vec2 d_tx = primitive_vert_expander(in_txuv[i], in_txuv[li], in_txuv[ri]);

    // Get ratio of relative expansion sizes
    float n_gl = length(d_gl);
    float n_tx = length(d_tx);

    // Normalize directions, then scale by same amount, being careful
    // to scale in render-side and uv-side as relatively the same
    d_gl = (d_gl / n_gl) * expansion_dist;
    d_tx = (d_tx / n_tx) * (n_tx / n_gl) * expansion_dist;
    
    // Output expanded triangles
    gl_Position = vec4(gl_in[i].gl_Position.xy + d_gl, 0, 1);
    out_txuv    = in_txuv[i] + d_tx;

    // Emit primitive vertices as same, given that we're just passing through
    gl_PrimitiveID = gl_PrimitiveIDIn;
    EmitVertex();
  }
}