#version 330 core

uniform sampler2D qb_blur_sampler;

layout (location = 0) out vec4 out_color;

layout (std140) uniform qb_blur_args
{
  bool horizontal;
} blur_args;

in VertexData
{
	vec2 tex;
} o;

const float weights[] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

void main()
{
  vec2 tex_offset = 1.0 / textureSize(qb_blur_sampler, 0); // gets size of single texel
  vec3 result = texture(qb_blur_sampler, o.tex).rgb * weights[0];
  if(blur_args.horizontal)
  {
    for(int i = 1; i < 5; ++i)
    {
      result += texture(qb_blur_sampler, o.tex + vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
      result += texture(qb_blur_sampler, o.tex - vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
    }
  } else {
    for(int i = 1; i < 5; ++i)
    {
      result += texture(qb_blur_sampler, o.tex + vec2(0.0, tex_offset.y * i)).rgb * weights[i];
      result += texture(qb_blur_sampler, o.tex - vec2(0.0, tex_offset.y * i)).rgb * weights[i];
    }
  }
  out_color = vec4(result, 1.0);
}