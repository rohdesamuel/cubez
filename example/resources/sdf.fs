#version 330 core

layout (location = 0) out vec4 out_color;

uniform sampler3D qb_layer_sampler;

layout (std140) uniform qb_sdf_fargs
{
  mat4 vp_inv;
  vec3 eye;
} f_args;

in VertexData
{
	vec2 pos;
	vec2 tex;
} o;


const int MAX_MARCHING_STEPS = 255;
const float MIN_DIST = 0.0;
const float MAX_DIST = 100.0;
const float EPSILON = 0.0001;

vec2 sphereUV(vec3 p) {
  return vec2(0.5f + atan(p.z, p.x) / 6.28318531f, 0.5 - asin(p.y) / 3.14159265f);
}

float intersectSDF(float distA, float distB) {
  return max(distA, distB);
}

float unionSDF(float distA, float distB) {
  return min(distA, distB);
}

float differenceSDF(float distA, float distB) {
  return max(distA, -distB);
}

float hash(float p) {
  p = fract(p * 0.011); p *= p + 7.5; p *= p + p; return fract(p);
}
float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.13); p3 += dot(p3, p3.yzx + 3.333); return fract((p3.x + p3.y) * p3.z);
}
float noise(float x) {
  float i = floor(x); float f = fract(x); float u = f * f * (3.0 - 2.0 * f); return mix(hash(i), hash(i + 1.0), u);
}
float noise(vec2 x) {
  vec2 i = floor(x); vec2 f = fract(x); float a = hash(i); float b = hash(i + vec2(1.0, 0.0)); float c = hash(i + vec2(0.0, 1.0)); float d = hash(i + vec2(1.0, 1.0)); vec2 u = f * f * (3.0 - 2.0 * f); return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
float noise(vec3 x) {
  const vec3 step = vec3(110, 241, 171); vec3 i = floor(x); vec3 f = fract(x); float n = dot(i, step); vec3 u = f * f * (3.0 - 2.0 * f); return mix(mix(mix(hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x), mix(hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y), mix(mix(hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x), mix(hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);

}

#define DEFINE_FBM(name, OCTAVES) float name(vec3 x) { float v = 0.0; float a = 0.5; vec3 shift = vec3(100); for (int i = 0; i < OCTAVES; ++i) { v += a * noise(x); x = x * 2.0 + shift; a *= 0.5; } return v; }
DEFINE_FBM(fbm3, 3)
DEFINE_FBM(fbm5, 5)
DEFINE_FBM(fbm6, 6)

float sphereSDF(vec3 p, vec3 c, float r) {
  return length(p - c) - r;
}

// polynomial smooth min (k = 0.1);
float smin( float a, float b, float k )
{
    float h = max( k-abs(a-b), 0.0 )/k;
    return min( a, b ) - h*h*k*(1.0/4.0);
}

float texSDF(vec3 p) {
  vec3 tex_boundary = vec3(0.5, 0.5, 0.5);
  vec3 tex_shell = vec3(0.005, 0.005, 0.005);
  vec3 di = abs(p) - tex_boundary + tex_shell;
  
  float res = length(max(di + tex_shell, 0.0));
  float c = max(di.x, max(di.y, di.z));
  if (c < 0) {
    p += tex_boundary;//vec3(0.f, 0.f, 0.f);

    float d_0 = (texture(qb_layer_sampler, p).r);
    float d_1 = (texture(qb_layer_sampler, p + vec3(EPSILON, 0, 0)).r);
    float d_2 = (texture(qb_layer_sampler, p + vec3(-EPSILON, 0, 0)).r);
    float d_3 = (texture(qb_layer_sampler, p + vec3(0, EPSILON, 0)).r);
    float d_4 = (texture(qb_layer_sampler, p + vec3(0, -EPSILON, 0)).r);
    float d_5 = (texture(qb_layer_sampler, p + vec3(0, 0, EPSILON)).r);
    float d_6 = (texture(qb_layer_sampler, p + vec3(0, 0, -EPSILON)).r);
    res = (d_0 + d_1 + d_2 + d_3 + d_4 + d_5 + d_6) / 7.0f;//sphereSDF(p, tex_boundary, 0.5f);////smin(d_0, sphereSDF(p), 0.1);//;
  }
  return res;
}

float shellSDF(vec3 p) {
  return abs(sphereSDF(p, vec3(0), 64.0f)) - 0.1;
}

float cubeSDF(vec3 p) {
  // If d.x < 0, then -1 < p.x < 1, and same logic applies to p.y, p.z
  // So if all components of d are negative, then p is inside the unit cube
  vec3 d = abs(p) - vec3(1.0, 1.0, 1.0);

  // Assuming p is inside the cube, how far is it from the surface?
  // Result will be negative or zero.
  float insideDistance = min(max(d.x, max(d.y, d.z)), 0.0);

  // Assuming p is outside the cube, how far is it from the surface?
  // Result will be positive or zero.
  float outsideDistance = length(max(d, 0.0));

  return insideDistance + outsideDistance;
}

float sceneSDF(vec3 p) {
  return texSDF(p);
}

float sphere_trace(vec3 p, vec3 d, float push, float max, float threshold) {
  float T = push;
  vec3 P = p + d * T;
  for (int i = 0; i < max; ++i) {
    float S = sceneSDF(P);
    T += S;
    P = p + d * T;    
    if ((T > MAX_DIST) || (S < threshold)) {
      break;
    }
  }
  return T;
}

float aabbSDF(vec3 p) {
  vec3 tex_boundary = vec3(0.5, 0.5, 0.5);
  vec3 tex_shell = vec3(0.005, 0.005, 0.005);
  vec3 di = abs(p) - tex_boundary + tex_shell;
  
  float res = length(max(di + tex_shell, 0.0));
  float c = max(di.x, max(di.y, di.z));
  if (c < 0) {
    return 0;
  }
  return res;
}

float voxel_sceneSDF(vec3 p) {
  return aabbSDF(p);
}

vec4 sphere_trace_voxel(vec3 p, vec3 d, float push, float max, float threshold) {
  float T = push;
  vec3 P = p + d * T;
  for (int i = 0; i < max; ++i) {
    float S = voxel_sceneSDF(P);    
    if (S < threshold) {
      P += vec3(0.5);
      for (int j = 0; j < 1000; ++j) {
        vec4 voxel = texture(qb_layer_sampler, P);
        if (voxel.a > 0) {
          return voxel;
        }
        P += d * 0.0005f;
      }
      break;
    }
    if (T > MAX_DIST) {
      break;
    }

    T += S;
    P = p + d * T;
  }
  return vec4(0);
}

vec3 rayDirection(vec2 size, vec2 screen) {
  vec4 screen_homogenous = vec4(2.0f * screen / size - vec2(1.0f), 1.0, 1.0);

  vec4 world_coord = f_args.vp_inv * screen_homogenous;
  /*world_coord.w = 1.0f / world_coord.w;
  world_coord.x *= world_coord.w;
  world_coord.y *= world_coord.w;
  world_coord.z *= world_coord.w;*/

  return normalize(vec3(world_coord));
}

vec3 estimateNormal(vec3 p) {
  return normalize(vec3(
    sceneSDF(vec3(p.x + EPSILON, p.y, p.z)) - sceneSDF(vec3(p.x - EPSILON, p.y, p.z)),
    sceneSDF(vec3(p.x, p.y + EPSILON, p.z)) - sceneSDF(vec3(p.x, p.y - EPSILON, p.z)),
    sceneSDF(vec3(p.x, p.y, p.z + EPSILON)) - sceneSDF(vec3(p.x, p.y, p.z - EPSILON))
  ));
}

vec3 phongContribForLight(vec3 k_d, vec3 k_s, float alpha, vec3 p, vec3 eye,
                          vec3 lightPos, vec3 lightIntensity) {
  vec3 N = estimateNormal(p);
  vec3 L = normalize(lightPos - p);
  vec3 V = normalize(eye - p);
  vec3 R = normalize(reflect(-L, N));

  float dotLN = dot(L, N);
  float dotRV = dot(R, V);

  if (dotLN < 0.0) {
    // Light not visible from this point on the surface
    return vec3(0.0, 0.0, 0.0);
  }

  if (dotRV < 0.0) {
    // Light reflection in opposite direction as viewer, apply only diffuse
    // component
    return lightIntensity * (k_d * dotLN);
  }
  return lightIntensity * (k_d * dotLN + k_s * pow(dotRV, alpha));
}

vec3 phongIllumination(vec3 k_a, vec3 k_d, vec3 k_s, float alpha, vec3 p, vec3 eye) {
  const vec3 ambientLight = 0.5 * vec3(1.0, 1.0, 1.0);
  vec3 color = ambientLight * k_a;

  vec3 light1Pos = vec3(4.0, 2.0, 4.0);
  vec3 light1Intensity = vec3(0.4, 0.4, 0.4);

  color += phongContribForLight(k_d, k_s, alpha, p, eye,
                                light1Pos,
                                light1Intensity);

  vec3 light2Pos = vec3(2.0, 2.0, 2.0);
  vec3 light2Intensity = vec3(0.4, 0.4, 0.4);

  color += phongContribForLight(k_d, k_s, alpha, p, eye,
                                light2Pos,
                                light2Intensity);
  return color;
}

void main()
{		
  vec2 viewport = vec2(2, 2);
  vec3 dir = rayDirection(viewport, o.pos + vec2(1.0, 1.0));
  vec3 eye = f_args.eye;
  //float dist = shortestDistanceToSurface(eye, dir, MIN_DIST, MAX_DIST) / MAX_DIST;
  out_color = sphere_trace_voxel(eye, dir, MIN_DIST, MAX_MARCHING_STEPS, EPSILON);
  return;

  
  #if 0
  if (dist > MAX_DIST) {
    // Didn't hit anything
    out_color = vec4(0.0, 0.0, 0.0, 0.0);
    return;
  }

  // The closest point on the surface to the eyepoint along the view ray
  vec3 p = eye + dist * dir;

  vec3 K_a = vec3(0.2, 0.2, 0.2);
  vec3 K_d = vec3(0.7, 0.2, 0.2);
  vec3 K_s = vec3(1.0, 1.0, 1.0);
  float shininess = 10.0;

  vec3 color = phongIllumination(K_a, K_d, K_s, shininess, p, eye);

  out_color = vec4(color, 1.0);
  #endif
}