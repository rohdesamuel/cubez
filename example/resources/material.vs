#version 330 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_tex;

layout (std140) uniform qb_camera
{
    mat4 vp;
} camera;

layout (std140) uniform qb_model
{
    mat4 m;
} model;

out VertexData
{
	vec3 norm;
	vec2 tex;
} o;

void main() {
  o.tex = in_tex;
  o.norm = in_norm;
  gl_Position = (camera.vp * model.m) * vec4(in_pos, 1.0);
}