#version 330 core

uniform sampler2D tex_sampler[32];

layout (location = 0) out vec4 out_color;

in VertexData
{
  flat vec4 col;
  vec2 tex;
  flat float tex_id;
} o;

void main() {
  int index = int(o.tex_id);
  out_color = texture(tex_sampler[index], o.tex) * o.col;
}