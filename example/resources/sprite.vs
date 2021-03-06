#version 330 core

layout (location = 0) in vec3  in_pos;
layout (location = 1) in vec4  in_col;
layout (location = 2) in vec2  in_tex;
layout (location = 3) in vec2  in_scale;
layout (location = 4) in float in_rot;
layout (location = 5) in float in_tex_id;

layout (std140) uniform Camera
{
    mat4 projection;
} camera;

out VertexData
{
	flat vec4 col;
  vec2 tex;
} o;

void main() {
  o.col = in_col;
  o.tex = in_tex;

  mat4 modelview = mat4(vec4(in_scale.x, 0.0,        0.0,      0.0),
                        vec4(0.0,        in_scale.y, 0.0,      0.0),
                        vec4(0.0,        0.0,        1.0,      0.0),
                        vec4(in_pos.x,   in_pos.y,   in_pos.z, 1.0));
  gl_Position =  (camera.projection * modelview) * vec4(in_pos, 1.0);
}