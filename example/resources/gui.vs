#version 330 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_col;
layout (location = 2) in vec2 in_tex;

layout (std140) uniform Camera
{
    mat4 projection;
} camera;

layout (std140) uniform Model
{
  mat4 modelview;
	vec4 color;  
	int render_mode;
  float radius;
  vec2 size;
} model;

out VertexData
{
	flat vec4 col;
	vec2 tex;
	flat int render_mode;
  flat float radius;
  flat vec2 size;
} o;

void main() {
  o.tex = in_tex;
  o.col = model.color;
  o.render_mode = model.render_mode;
  o.radius = model.radius;
  o.size = model.size;
  gl_Position =  (camera.projection * model.modelview) * vec4(in_pos, 1.0);
}