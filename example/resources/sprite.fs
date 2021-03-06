#version 330 core

uniform sampler2D tex_sampler;

layout (location = 0) out vec4 out_color;

in VertexData
{
	flat vec4 col;
  vec2 tex;
} o;

float udRoundBox(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, q.y),0.0) - r;
}

void main() {
  //if (o.render_mode == 0) { // RENDER_MODE_SOLID
	out_color = o.col;
  //} else if (o.render_mode == 1) { // RENDER_MODE_IMAGE
	out_color = texture2D(tex_sampler, o.tex);// * o.col;
	//  out_color.a = o.col.a;
  //}

  //out_color = vec4(1.0, 0.0, 1.0, 1.0);
}