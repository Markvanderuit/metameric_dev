#version 460 core

void main() {
  const vec3 positions[3] = vec3[3](
    vec3(1.f, 1.f, 0.f),
    vec3(-1.f, 1.f, 0.f),
    vec3(0.f, -1.f, 0.f)
  );
  
  gl_Position = vec4(positions[gl_VertexID], 1.f);
}