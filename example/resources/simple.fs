#version 330 core

uniform sampler2D qb_texture_unit_1;
uniform sampler2D qb_texture_unit_2;

layout (location = 0) out vec4 out_color;

in VertexData
{
	vec4 col;
	vec2 tex;
} o;

void main() {
  out_color = o.col;//texture2D(qb_texture_unit_1, o.tex) * texture2D(qb_texture_unit_2, o.tex) * o.col;
}