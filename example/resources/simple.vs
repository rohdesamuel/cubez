#version 330 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_tex;
//layout (location = 3) in vec3 in_offset;

layout (std140) uniform Camera
{
    mat4 projection;
} camera;

layout (std140) uniform Model
{
    mat4 modelview;
} model;

out VertexData
{
	vec4 col;
	vec2 tex;
} o;

void main() {
  o.tex = in_tex;
  o.col = vec4(in_norm, 1.0);// vec4(1.0, 0.0, 0.0, 1.0);
  gl_Position = (camera.projection * model.modelview) * vec4(in_pos, 1.0);
}