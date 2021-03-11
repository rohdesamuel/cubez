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
}