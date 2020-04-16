#version 330 core

uniform sampler2D qb_material_albedo_map;
uniform sampler2D qb_material_normal_map;
uniform sampler2D qb_material_metallic_map;
uniform sampler2D qb_material_roughness_map;
uniform sampler2D qb_material_ao_map;
uniform sampler2D qb_material_emission_map;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 emission_color;

layout (std140) uniform qb_material
{
  vec3 albedo;
  float metallic;
  vec3 emission;
  float roughness;
} material;

// lights
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

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(qb_material_normal_map, o.tex).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(o.pos);
    vec3 Q2  = dFdy(o.pos);
    vec2 st1 = dFdx(o.tex);
    vec2 st2 = dFdy(o.tex);

    vec3 N   = normalize(o.norm);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
// ----------------------------------------------------------------------------
void main()
{		
    vec3 albedo     = pow(texture(qb_material_albedo_map, o.tex).rgb + material.albedo, vec3(2.2));
    float metallic  = texture(qb_material_metallic_map, o.tex).r + material.metallic;
    float roughness = texture(qb_material_roughness_map, o.tex).r + material.roughness;
    float ao        = texture(qb_material_ao_map, o.tex).r;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(lights.view_pos - o.pos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < kMaxDirectionalLights; ++i) 
    {
        qbLightDirectional light_directional = lights.directionals[i];

        // calculate per-light radiance
        vec3 L = normalize(-light_directional.dir);
        vec3 H = normalize(V + L);
        vec3 radiance = light_directional.brightness * light_directional.rgb;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 nominator    = NDF * G * F; 
        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001; // 0.001 to prevent divide by zero.
        vec3 specular = nominator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);        

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    for(int i = 0; i < kMaxPointLights; ++i) 
    {
        qbLightPoint light_point = lights.points[i];

        // calculate per-light radiance
        vec3 L = normalize(light_point.pos - o.pos);
        vec3 H = normalize(V + L);
        float distance = length(light_point.pos - o.pos);
        float attenuation = light_point.brightness / (distance * distance);
        vec3 radiance = light_point.rgb * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 nominator    = NDF * G * F; 
        float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001; // 0.001 to prevent divide by zero.
        vec3 specular = nominator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);        

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }   
    
    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 emissive = vec3(material.emission) + texture(qb_material_emission_map, o.tex).rgb;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    out_color = vec4(color, 1.0);
    emission_color = vec4(emissive, 1.0);
}