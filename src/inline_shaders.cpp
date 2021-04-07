#include "inline_shaders.h"

static const char* sprite_vs = R"(
#version 330 core

layout (location = 0) in vec2  in_pos;
layout (location = 1) in vec2  in_offset;
layout (location = 2) in vec4  in_col;
layout (location = 3) in vec2  in_tex;
layout (location = 4) in vec2  in_scale;
layout (location = 5) in float in_rot;
layout (location = 6) in float in_tex_id;

layout (std140) uniform Camera
{
    mat4 projection;
} camera;

out VertexData
{
	flat vec4 col;
  vec2 tex;
  flat float tex_id;
} o;

void main() {
  o.col = in_col;
  o.tex = in_tex;
  o.tex_id = in_tex_id;

  float c = cos(in_rot);
  float s = sin(in_rot);

  mat4 pos = mat4(vec4(1.0, 0.0, 0.0, 0.0),
                  vec4(0.0, 1.0, 0.0, 0.0),
                  vec4(0.0, 0.0, 1.0, 0.0),
                  vec4(in_offset.x, in_offset.y, 0.0, 1.0));

  mat4 rot = mat4(vec4(c, -s, 0.0, 0.0),
                  vec4(s,  c, 0.0, 0.0),
                  vec4(0.0, 0.0, 1.0, 0.0),
                  vec4(0.0, 0.0, 0.0, 1.0));

  mat4 scale = mat4(vec4(in_scale.x, 0.0,        0.0,      0.0),
                    vec4(0.0,        in_scale.y, 0.0,      0.0),
                    vec4(0.0,        0.0,        1.0,      0.0),
                    vec4(0.0,        0.0,        0.0,      1.0));

  gl_Position =  (camera.projection * pos * rot * scale) * vec4(in_pos.x, in_pos.y, 0.0, 1.0);
})";

static const char* sprite_fs = R"(
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
})";

static const char* presentpass_vs = R"(
#version 330 core

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inTexCoord;

out vec2 TexCoord;

void main() {
  TexCoord = inTexCoord;
  gl_Position = vec4(inPos, 0.0, 1.0);
})";

static const char* presentpass_fs = R"(
#version 330 core
in vec2 TexCoord;

out vec4 outFragColor;

uniform sampler2D tex_sampler;

void main() {
  outFragColor = texture2D(tex_sampler, TexCoord);
})";

static const char* gui_vs = R"(
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
})";

static const char* gui_fs = R"(
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
})";

const char* get_sprite_vs() {
  return sprite_vs;
}

const char* get_sprite_fs() {
  return sprite_fs;
}

const char* get_presentpass_vs() {
  return presentpass_vs;
}

const char* get_presentpass_fs() {
  return presentpass_fs;
}

const char* get_gui_vs() {
  return gui_vs;
}

const char* get_gui_fs() {
  return gui_fs;
}