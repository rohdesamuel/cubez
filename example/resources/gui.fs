#version 330 core

uniform sampler2D tex_sampler;

layout (location = 0) out vec4 out_color;

in VertexData
{
	vec4 col;
	vec2 tex;
} o;

void main() {
  out_color = o.col; //texture2D(tex_sampler, o.tex) * o.col;
}