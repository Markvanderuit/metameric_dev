b#version 460 core

#extension GL_ARB_shading_language_include : enable
#extension GL_GOOGLE_include_directive     : enable

#include "resources/shaders/spectrum/spectrum.glsl"

layout (location = 0) in vec3 gamut_value_in;
layout (location = 0) out vec3 color_value_out;

void main() {
  color_value_out = test_fun(gamut_value_in);
}