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
    mat4 rot;
} model;

out VertexData
{
	vec3 pos;
	vec3 norm;
	vec2 tex;
} o;

void main() {
  o.pos = vec3(model.m * vec4(in_pos, 1.0));
  o.tex = in_tex;
  o.norm = vec3(model.rot * vec4(in_norm, 1.0));
  gl_Position = (camera.vp * model.m) * vec4(in_pos, 1.0);
}