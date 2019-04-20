#version 330 core

uniform sampler2D tex_sampler;

layout (location = 0) out vec4 out_color;

in VertexData
{
	flat vec4 col;
	vec2 tex;
	flat int render_mode;
} o;

void main() {
  if (o.render_mode == 0) { // GUI_RENDER_MODE_SOLID
	out_color = o.col;
  } else if (o.render_mode == 1) { // GUI_RENDER_MODE_IMAGE
	out_color = texture2D(tex_sampler, o.tex);// * o.col;
	out_color.a = o.col.a;
  } else if (o.render_mode == 2) {  // GUI_RENDER_MODE_STRING
	out_color = vec4(1.0, 1.0, 1.0, texture2D(tex_sampler, o.tex)) * o.col;
  }
}