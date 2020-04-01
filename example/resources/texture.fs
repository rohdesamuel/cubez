#version 330 core
in vec2 TexCoord;

out vec4 outFragColor;

uniform sampler2D tex_sampler;

void main() {
  outFragColor = texture2D(tex_sampler, TexCoord);
}