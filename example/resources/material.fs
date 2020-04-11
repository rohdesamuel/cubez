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

struct qbLightPoint {
  vec3 rgb;
  float brightness;
  vec3 pos;  
  float radius;
};

struct qbLightDirectional {
  vec3 rgb;
  float brightness;
  vec3 dir;
};

struct qbLightSpot {
  vec3 rgb;
  float brightness;
  vec3 pos;
  float range;
  vec3 dir;
  float angle_deg;
};

const int kMaxDirectionalLights = 4;
const int kMaxPointLights = 32;
const int kMaxSpotLights = 8;

layout (std140) uniform qb_lights
{
  qbLightDirectional directionals[kMaxDirectionalLights];
  qbLightPoint points[kMaxPointLights];
  qbLightSpot spots[kMaxSpotLights];
	
  vec3 view_pos;
} lights;

in VertexData
{
	vec3 pos;
	vec3 norm;
	vec2 tex;
} o;


vec3 CalcDirLight(qbLightDirectional light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.dir);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    float metallic = texture(qb_material_metallic_map, o.tex).r;
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.metallic);
    // combine results
    vec3 ambient  = light.rgb  * vec3(material.albedo);
    vec3 diffuse  = light.rgb  * diff * vec3(material.albedo);
    vec3 specular = light.rgb * spec * vec3(material.albedo) * metallic;
    return light.brightness * (ambient + diffuse + specular);
}  

vec3 CalcPointLight(qbLightPoint light, vec3 normal, vec3 frag_pos, vec3 view_dir)
{
	vec3 lightDir = normalize(light.pos - frag_pos);
	
  // ambient shading
  vec3 ambient  = 0.1 * light.rgb * vec3(material.albedo);

  // diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
  vec3 diffuse  = light.rgb * vec3(material.albedo) * diff * vec3(texture2D(qb_material_albedo_map, o.tex));

	// specular shading
  float metallic = texture(qb_material_metallic_map, o.tex).r;
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(view_dir, reflectDir), 0.0), material.metallic);
  vec3 specular = light.rgb * spec * vec3(material.albedo) * metallic;

	// attenuation
	float distance    = length(light.pos - frag_pos);
	float attenuation = light.brightness / (distance * distance);

	// combine results	
	return attenuation * (diffuse + specular);
} 

void main() {

	// properties
	vec3 norm = normalize(o.norm);
	vec3 view_dir = normalize(lights.view_pos - o.pos);

	// phase 1: Directional lighting
	vec3 result = vec3(0, 0, 0);
  for(int i = 0; i < kMaxDirectionalLights; i++)
		result += CalcDirLight(lights.directionals[i], norm, view_dir);    

	// phase 2: Point lights
	for(int i = 0; i < kMaxPointLights; i++)
		result += CalcPointLight(lights.points[i], norm, o.pos, view_dir);    
	// phase 3: Spot light
	//result += CalcSpotLight(spotLight, norm, FragPos, view_dir);    
	out_color = vec4(result, 1.0);

  //out_color = material.albedo * texture2D(qb_material_albedo_map, o.tex);// * vec4(lights.points[0].rgb, 1);
}