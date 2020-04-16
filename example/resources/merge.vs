#version 330 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_tex;

out VertexData
{
	vec2 tex;
} o;

void main() {
  o.tex = in_tex;
  gl_Position = vec4(in_pos, 0.0, 1.0);
}