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

layout (std140, binding = 0) uniform Camera
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

  mat4 rot = mat4(vec4(c, -s, 0.0, 0.0),
                  vec4(s,  c, 0.0, 0.0),
                  vec4(0.0, 0.0, 1.0, 0.0),
                  vec4(0.0, 0.0, 0.0, 1.0));

  mat4 scale = mat4(vec4(in_scale.x, 0.0,        0.0,      0.0),
                    vec4(0.0,        in_scale.y, 0.0,      0.0),
                    vec4(0.0,        0.0,        1.0,      0.0),
                    vec4(0.0,        0.0,        0.0,      1.0));
  gl_Position =  camera.projection * ((rot * scale) * vec4(in_offset, 0.0, 1.0) + vec4(in_pos, 0.0, 0.0));
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
    vec3 col = texture(tex_sampler, TexCoord).rgb;
    outFragColor = vec4(col, 1.0);
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
