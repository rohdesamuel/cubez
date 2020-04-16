#version 330 core

uniform sampler2D qb_merge_sampler;
uniform sampler2D qb_scene_sampler;

layout (location = 0) out vec4 out_color;

layout (std140) uniform qb_merge_args
{
  bool bloom;
  float exposure;
} blur_args;

in VertexData
{
	vec2 tex;
} o;

void main()
{
  const float gamma = 2.2;
  vec3 hdrColor = texture(qb_scene_sampler, o.tex).rgb;      
  vec3 bloomColor = texture(qb_merge_sampler, o.tex).rgb;
  if(blur_args.bloom) {
    hdrColor += bloomColor; // additive blending
  }
  // tone mapping
  vec3 result = vec3(1.0) - exp(-hdrColor * blur_args.exposure);
  // also gamma correct while we're at it       
  result = pow(result, vec3(1.0 / gamma));
  out_color = vec4(result, 1.0);
}