#version 330 core

uniform sampler2D tex_sampler;

layout (location = 0) out vec4 out_color;

in VertexData
{
	flat vec4 col;
	vec2 tex;
	flat int render_mode;
  flat float radius;
  flat vec2 size;
} o;

float udRoundBox(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, q.y),0.0) - r;
}

void main() {
  if (o.render_mode == 0) { // GUI_RENDER_MODE_SOLID
	  out_color = o.col;
  } else if (o.render_mode == 1) { // GUI_RENDER_MODE_IMAGE
	  out_color = texture2D(tex_sampler, o.tex) * o.col;
	  out_color.a = o.col.a;
  } else if (o.render_mode == 2) {  // GUI_RENDER_MODE_STRING
	  out_color = vec4(1.0, 1.0, 1.0, texture2D(tex_sampler, o.tex)) * o.col;
  }

  vec2 p = o.tex * o.size;
  vec2 c = o.size * 0.5;
  
  // Clamping here allows for small antialiasing around the edge.
  // See https://mortoray.com/2015/06/19/antialiasing-with-a-signed-distance-field/.
  float dist = udRoundBox(p - c, c - o.radius, o.radius);
  out_color.a *= clamp( 0.5 - dist, 0, 1 );
}