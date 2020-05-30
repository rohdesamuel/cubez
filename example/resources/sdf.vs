#version 330 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_tex;

layout (std140) uniform qb_sdf_vargs
{
    vec4 unused;
} v_args;

out VertexData
{
	vec2 pos;
	vec2 tex;
} o;

void main() {
  o.pos = in_pos;
  o.tex = in_tex;
  gl_Position = vec4(vec3(in_pos, 0.0f), 1.0f);
}