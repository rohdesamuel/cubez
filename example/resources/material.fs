#version 330 core

uniform sampler2D qb_material_albedo_map;
uniform sampler2D qb_material_normal_map;
uniform sampler2D qb_material_metallic_map;
uniform sampler2D qb_material_roughness_map;
uniform sampler2D qb_material_ao_map;
uniform sampler2D qb_material_emission_map;

layout (location = 0) out vec4 out_color;

layout (std140) uniform qb_material
{
    vec4 albedo;
	float metallic;
	float roughness;
	vec4 emission;
} material;

layout (std140) uniform qb_lights
{
    vec4 albedo;
	float metallic;
	float roughness;
	vec4 emission;
} lights;

in VertexData
{
	vec3 norm;
	vec2 tex;
} o;

void main() {
  out_color = material.albedo * texture2D(qb_material_albedo_map, o.tex);
}