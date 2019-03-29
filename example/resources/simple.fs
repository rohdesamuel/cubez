#version 330 core

uniform sampler2D tex_sampler_1;
uniform sampler2D tex_sampler_2;

layout (location = 0) out vec4 out_color;

in VertexData
{
	vec4 col;
	vec2 tex;
} o;

void main() {
  out_color = texture2D(tex_sampler_1, o.tex) * texture2D(tex_sampler_2, o.tex) * o.col;
}