/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <cubez/mesh.h>
#include <cubez/render.h>
#include "mesh_builder.h"
#include "shader.h"

#include <GL/glew.h>

#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <string>
#include <tuple>
#include <array>
#include <vector>
#include <cubez/cubez.h>
#include <assert.h>
#include <algorithm>

#include "render_defs.h"
#include <cglm/struct.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif

qbModel qb_model_load(const char* model_name, const char* filename) {
  MeshBuilder builder = MeshBuilder::FromFile(filename);
  return builder.Model(QB_DRAW_MODE_TRIANGLES);
}

struct qbModelGroup_* qb_model_upload(qbModel model) {
  qbModelGroup ret;
  qb_modelgroup_create(&ret);
  qb_modelgroup_upload(ret, model, nullptr);
  return ret;
}

void qb_model_create(qbModel* model_ref, qbModelAttr attr) {
  qbModel model = *model_ref = new qbModel_{};
  model->name = STRDUP(attr->name);

  model->collider_count = attr->collider_count;
  model->colliders = new qbCollider[model->collider_count];
  memcpy(model->colliders, attr->colliders, (model->collider_count) * sizeof(qbCollider));
  
  model->mesh_count = attr->mesh_count;
  model->meshes = new qbMesh[model->mesh_count];
  memcpy(model->meshes, attr->meshes, (model->mesh_count) * sizeof(qbMesh));
  (*model_ref)->mode = attr->meshes[0]->mode;
}

void qb_model_destroy(qbModel* model) {
  qbModel m = *model;
  if (m->meshes) {
    for (uint32_t i = 0; i < (*model)->mesh_count; ++i) {
      qb_mesh_destroy(&m->meshes[i]);
    }
    delete[] m->meshes;
  }

  if (m->colliders) {
    for (uint32_t i = 0; i < (*model)->collider_count; ++i) {
      qb_collider_destroy(&m->colliders[i]);
    }
    delete[] m->colliders;
  }
  delete *model;
  *model = nullptr;
}

qbResult qb_collider_destroy(qbCollider* collider) {
  free((*collider)->vertices);
  delete *(collider);
  return QB_OK;
}

bool qb_model_collides(vec3 a_origin, vec3 b_origin, qbModel a, qbModel b) {
  assert(false && "unimplemented");
  return false;
}

qbResult qb_mesh_tobuffer(qbMesh mesh, qbMeshBuffer* buffer) {
  qbRenderer renderer = qb_renderer();
  *buffer = renderer->meshbuffer_create(renderer, mesh);
  return QB_OK;
}

qbResult qb_mesh_destroy(qbMesh* mesh) {
  delete[](*mesh)->vertices;
  delete[](*mesh)->indices;
  delete[](*mesh)->normals;
  delete[](*mesh)->uvs;
  delete *mesh;
  *mesh = nullptr;
  return QB_OK;
}

qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name) {
  qbMaterial m = *material = new qbMaterial_;
  m->name = STRDUP(material_name);
  m->ext = attr->ext;

  m->albedo_map = attr->albedo_map;
  m->normal_map = attr->normal_map;
  m->metallic_map = attr->metallic_map;
  m->roughness_map = attr->roughness_map;
  m->ao_map = attr->ao_map;
  m->emission_map = attr->emission_map;

  m->albedo = attr->albedo;
  m->metallic = attr->metallic;
  m->roughness = attr->roughness;
  m->emission = attr->emission;

  return QB_OK;
}

qbResult qb_material_destroy(qbMaterial* material) {
  qbMaterial m = *material;
  qb_renderext_destroy(&(*material)->ext);

  delete m;
  *material = nullptr;

  return QB_OK;
}

vec3s qb_collider_support(const qbCollider_* collider, const qbTransform_* transform, vec3s dir) {
  // Rotate dir to match the orientation of the collider.
  mat4s inv_rot = glms_mat4_inv(transform->orientation);
  dir = glms_vec3(glms_mat4_mulv(inv_rot, glms_vec4(dir, 0.f)));

  float max_dot = std::numeric_limits<float>::lowest();
  int found = 0;
  for (int i = 0; i < collider->vertex_count; ++i) {
    float dot = glms_vec3_dot(collider->vertices[i], dir);
    if (dot > max_dot) {
      max_dot = dot;
      found = i;
    }
  }

  vec3s s_local = collider->vertices[found];

  // Returned support in world-space.
  return glms_vec3_add(glms_vec3(glms_mat4_mulv(transform->orientation, glms_vec4(s_local, 0.f))), transform->position);
}

struct qbPortal_ {
  vec3s v0, v1, v2, v3;
};

struct MinkowskiDifference {
  const qbCollider_* a;
  const qbCollider_* b;
  const qbTransform_* a_transform;
  const qbTransform_* b_transform;
};

float plane_plane_distance(const vec3s* p1, const vec3s* p2, const vec3s* n) {
  // The distance between two parallel planes is the same as subtracting their
  // distance from a known point on each plane to the origin.
  // http://wwwf.imperial.ac.uk/metric/metric_public/vectors/vector_coordinate_geometry/distance_between_planes.html
  float norm = glms_vec3_norm(*n);
  float d_1 = glms_vec3_dot(*p1, *n) / norm;
  float d_2 = glms_vec3_dot(*p2, *n) / norm;
  return std::abs(d_1 - d_2);
}

qbBool qb_collider_checkaabb(const qbCollider_* a, const qbCollider_* b,
                           const qbTransform_* a_t, const qbTransform_* b_t) {
  return
    a_t->position.x + a->min.x <= b_t->position.x + b->max.x &&
    a_t->position.x + a->max.x >= b_t->position.x + b->min.x &&
    a_t->position.y + a->min.y <= b_t->position.y + b->max.y &&
    a_t->position.y + a->max.y >= b_t->position.y + b->min.y &&
    a_t->position.z + a->min.y <= b_t->position.z + b->max.z &&
    a_t->position.z + a->max.z >= b_t->position.z + b->min.z;
}

qbBool qb_ray_checktri(const qbRay_* ray, const vec3s* v0, const vec3s* v1, const vec3s* v2,
                     vec3s* intersection_point, float* t) {
  const float EPSILON = 0.0000001f;

  vec3s edge1, edge2, h, s, q;
  float a, f, u, v;
  edge1 = glms_vec3_sub(*v1, *v0);
  edge2 = glms_vec3_sub(*v2, *v0);
  h = glms_vec3_cross(ray->dir, edge2);
  a = glms_vec3_dot(edge1, h);
  if (a > -EPSILON && a < EPSILON)
    return QB_FALSE;    // This ray is parallel to this triangle.
  f = 1.0f / a;
  s = glms_vec3_sub(ray->orig, *v0);
  u = f * glms_vec3_dot(s, h);
  if (u < 0.0f || u > 1.0f)
    return QB_FALSE;
  q = glms_vec3_cross(s, edge1);
  v = f * glms_vec3_dot(ray->dir, q);
  if (v < 0.0f || u + v > 1.0f)
    return QB_FALSE;
  // At this stage we can compute t to find out where the intersection point is on the line.
  *t = f * glms_vec3_dot(edge2, q);
  if (*t > EPSILON) { // ray intersection
    *intersection_point = glms_vec3_add(ray->orig, glms_vec3_scale(ray->dir, *t));
    return QB_TRUE;
  } else {
    // This means that there is a line intersection but not a ray intersection.
    return QB_FALSE;
  }
}

qbBool qb_ray_checkaabb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                      float* tmin_out, float* tmax_out) {
  vec3s max = glms_vec3_add(t->position, c->max);
  vec3s min = glms_vec3_add(t->position, c->min);

  float tmin = (min.x - r->orig.x) / r->dir.x;
  float tmax = (max.x - r->orig.x) / r->dir.x;

  if (tmin > tmax) std::swap(tmin, tmax);

  float tymin = (min.y - r->orig.y) / r->dir.y;
  float tymax = (max.y - r->orig.y) / r->dir.y;

  if (tymin > tymax) std::swap(tymin, tymax);

  if ((tmin > tymax) || (tymin > tmax))
    return QB_FALSE;

  if (tymin > tmin)
    tmin = tymin;

  if (tymax < tmax)
    tmax = tymax;

  float tzmin = (min.z - r->orig.z) / r->dir.z;
  float tzmax = (max.z - r->orig.z) / r->dir.z;

  if (tzmin > tzmax) std::swap(tzmin, tzmax);

  if ((tmin > tzmax) || (tzmin > tmax))
    return QB_FALSE;

  if (tzmin > tmin)
    tmin = tzmin;

  if (tzmax < tmax)
    tmax = tzmax;

  if (tmin_out) {
    *tmin_out = tmin;
  }
  if (tmax_out) {
    *tmax_out = tmax;
  }
  return QB_TRUE;
}

qbBool qb_ray_checkobb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                     float* tmin, float* tmax) {
  qbRay_ local_ray{};
  mat4s inv_orientation = glms_mat4_inv(t->orientation);
  local_ray.dir = glms_mat4_mulv3(inv_orientation, r->dir, 1.f);
  local_ray.orig = glms_mat4_mulv3(inv_orientation, r->orig, 0.f);

  return qb_ray_checkaabb(c, t, &local_ray, tmin, tmax);
}

qbBool qb_collider_mpr(const qbCollider_* a, const qbCollider_* b,
                     const qbTransform_* a_transform, const qbTransform_* b_transform) {
  const static float kEpsilon = 0.0000001f;

  MinkowskiDifference m = { a, b, a_transform, b_transform };

  auto mpr_phase_one = [&m](qbPortal_* out) {
    vec3s v = glms_vec3_sub(glms_vec3_add(m.a_transform->position, m.a->center),
                            glms_vec3_add(m.b_transform->position, m.b->center));
    vec3s a = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, glms_vec3_negate(v)),
                            qb_collider_support(m.b, m.b_transform, v));
    
    vec3s v_a = glms_vec3_cross(v, a);
    if (glms_vec3_eq_eps(v_a, 0.f)) {
      if (glms_vec3_dot(v, a) < 0.f) {
        return 1;
      } else {
        return -1;
      }
    }

    vec3s b = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, v_a),
                            qb_collider_support(m.b, m.b_transform, glms_vec3_negate(v_a)));
    
    vec3s avb = glms_vec3_cross(glms_vec3_sub(a, v), glms_vec3_sub(b, v));
    if (glms_vec3_eq_eps(avb, 0.0f)) {
      return -1;
    }

    vec3s c = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, avb),
                            qb_collider_support(m.b, m.b_transform, glms_vec3_negate(avb)));
    out->v0 = v;
    out->v1 = a;
    out->v2 = b;
    out->v3 = c;
    return 0;
  };

  auto mpr_phase_two = [&m](qbPortal_* p) {
    vec3s r = glms_vec3_negate(p->v0);

    bool done;
    do {
      done = true;
      vec3s n_vab = glms_vec3_cross(glms_vec3_sub(p->v1, p->v0),
                                    glms_vec3_sub(p->v2, p->v0));

      vec3s n_vbc = glms_vec3_cross(glms_vec3_sub(p->v2, p->v0),
                                    glms_vec3_sub(p->v3, p->v0));

      vec3s n_vca = glms_vec3_cross(glms_vec3_sub(p->v3, p->v0),
                                    glms_vec3_sub(p->v1, p->v0));

      if (glms_vec3_dot(r, n_vab) > 0.f) {
        done = false;
        p->v3 = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, n_vab),
                              qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n_vab)));
        vec3s tmp = p->v1;
        p->v1 = p->v2;
        p->v2 = tmp;
      } else if (glms_vec3_dot(r, n_vbc) > 0.f) {
        done = false;
        p->v1 = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, n_vbc),
                              qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n_vbc)));
        vec3s tmp = p->v2;
        p->v2 = p->v3;
        p->v3 = tmp;
      } else if (glms_vec3_dot(r, n_vca) > 0.f) {
        done = false;
        p->v2 = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, n_vca),
                              qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n_vca)));
        vec3s tmp = p->v3;
        p->v3 = p->v1;
        p->v1 = tmp;
      }
    } while (!done);
  };

  auto mpr_phase_three = [&m](qbPortal_* p) {
    while (true) {
      vec3s n = glms_vec3_cross(glms_vec3_sub(p->v3, p->v1),
                                glms_vec3_sub(p->v2, p->v1));
      if (glms_vec3_dot(p->v1, n) > 0.f) {
        return true;
      }

      if (std::abs(glms_vec3_dot(p->v1, n)) <= 0.0000001f) {
        return false;
      }

      vec3s p_ = glms_vec3_sub(qb_collider_support(m.a, m.a_transform, n),
                               qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n)));

      if (glms_vec3_dot(p_, n) < 0.f) {
        return false;
      }

      if (glms_vec3_dot(p->v0, glms_vec3_cross(p_, p->v1)) > 0.f) {
        if (glms_vec3_dot(p->v0, glms_vec3_cross(p_, p->v2)) < 0.f) {
          p->v3 = p_;
        } else {
          p->v1 = p_;
        }
      } else {
        if (glms_vec3_dot(p->v0, glms_vec3_cross(p_, p->v3)) > 0.f) {
          p->v2 = p_;
        } else {
          p->v1 = p_;
        }
      }
    }
  };

  qbPortal_ portal;
  int ray_case = mpr_phase_one(&portal);
  if (ray_case == 1) {
    return QB_TRUE;
  } else if (ray_case == -1) {
    return QB_FALSE;
  }

  mpr_phase_two(&portal);
  return mpr_phase_three(&portal);
}

qbBool qb_collider_checkmesh(const qbCollider_* a, const qbCollider_* b,
                      const qbTransform_* a_t, const qbTransform_* b_t) {
  return qb_collider_mpr(a, b, a_t, b_t);
}

qbBool qb_collider_check(const qbCollider_* a, const qbCollider_* b,
                       const qbTransform_* a_t, const qbTransform_* b_t) {
  if (qb_collider_checkaabb(a, b, a_t, b_t)) {
    return qb_collider_checkmesh(a, b, a_t, b_t);
  }
  return QB_FALSE;
}

qbBool qb_ray_checkplane(const qbRay_* ray, const qbRay_* plane, float* t) {
  // assuming vectors are all normalized
  float denom = glms_vec3_dot(plane->dir, ray->dir);
  if (denom > 1e-6) {
    vec3s p0l0 = glms_vec3_sub(plane->orig, ray->orig);
    *t = glms_vec3_dot(p0l0, plane->dir) / denom;
    return (t >= 0);
  }

  return QB_FALSE;
}

qbBool qb_collider_checkray(const qbCollider_* c, const qbTransform_* transform, const qbRay_* r, float* t) {
  // http://www.m-hikari.com/ams/ams-2013/ams-101-104-2013/koptelovAMS101-104-2013.pdf
  if (!qb_ray_checkaabb(c, transform, r, nullptr, nullptr)) {
    return QB_FALSE;
  }

  if (c->vertex_count == 0) {
    return QB_TRUE;
  }

  const static float kDistanceEpsilon = 0.0001f;
  const static float kAngleEpsilon = 0.0001f;

  vec3s from = glms_vec3_normalize(r->dir);
  vec3s to = vec3s{ 0.f, 0.f, 1.f };
  float rotation_angle = (float)std::acos(glms_vec3_dot(from, to));

  mat4s rotation_mat = GLMS_MAT4_IDENTITY_INIT;
  if (std::abs(rotation_angle) >= kAngleEpsilon) {
    vec3s rotation_axis = glms_vec3_normalize(glms_vec3_cross(from, to));
    rotation_mat = glms_rotate_make(rotation_angle, rotation_axis);
  }
  vec3s translation_vec = glms_vec3_negate(r->orig);

  std::vector<vec3s> p_neg_v;
  std::vector<vec3s> p_pos_v;
  p_neg_v.reserve(c->vertex_count);
  p_pos_v.reserve(c->vertex_count);
  for (int i = 0; i < c->vertex_count; ++i) {
    vec3s p = glms_vec3_add(glms_mat4_mulv3(transform->orientation, c->vertices[i], 1.f), transform->position);
    p = glms_mat4_mulv3(rotation_mat, glms_vec3_add(translation_vec, p), 1.f);
    if (p.y >= 0) {
      p_pos_v.push_back(p);
    } else {
      p_neg_v.push_back(p);
    }
  }

  if (p_neg_v.empty() || p_pos_v.empty()) {
    return QB_FALSE;
  }

  std::vector<vec3s> p_zero_v;
  p_zero_v.reserve(p_neg_v.size() * p_pos_v.size());
  for (size_t i = 0; i < p_neg_v.size(); ++i) {
    const auto& p_neg = p_neg_v[i];
    for (size_t j = i; j < p_pos_v.size(); ++j) {
      const auto& p_pos = p_pos_v[j];

      qbRay_ r{ p_neg, glms_vec3_normalize(glms_vec3_sub(p_pos, p_neg)) };
      qbRay_ plane{ vec3s{}, vec3s{0.f, 1.f, 0.f} };
      float t = 0.f;
      qb_ray_checkplane(&r, &plane, &t);
      p_zero_v.push_back(glms_vec3_add(r.orig, glms_vec3_scale(r.dir, t)));
    }
  }

  p_neg_v.clear();
  p_pos_v.clear();
  p_neg_v.reserve(p_zero_v.size());
  p_pos_v.reserve(p_zero_v.size());
  for (int i = 0; i < p_zero_v.size(); ++i) {
    vec3s p = p_zero_v[i];
    if (p.x >= 0) {
      p_pos_v.push_back(p);
    } else {
      p_neg_v.push_back(p);
    }
  }

  if (p_neg_v.empty() || p_pos_v.empty()) {
    return QB_FALSE;
  }

  p_zero_v.clear();
  p_zero_v.reserve(p_neg_v.size() * p_pos_v.size());
  for (size_t i = 0; i < p_neg_v.size(); ++i) {
    const auto& p_neg = p_neg_v[i];
    for (size_t j = i; j < p_pos_v.size(); ++j) {
      const auto& p_pos = p_pos_v[j];

      qbRay_ r{ p_neg, glms_vec3_normalize(glms_vec3_sub(p_pos, p_neg)) };
      qbRay_ plane{ vec3s{}, vec3s{ 1.f, 0.f, 0.f } };
      float t = 0.f;
      qb_ray_checkplane(&r, &plane, &t);
      p_zero_v.push_back(glms_vec3_add(r.orig, glms_vec3_scale(r.dir, t)));
    }
  }

  std::sort(p_zero_v.begin(), p_zero_v.end(), [](const vec3s& a, const vec3s& b) {
    return a.z < b.z;
  });

  *t = p_zero_v.front().z;
  return QB_TRUE;
}

struct qbModelGroup_* qb_draw_cube(float size_x, float size_y, float size_z, qbDrawMode mode, qbCollider* collider) {
  MeshBuilder builder = MeshBuilder::Box(size_x, size_y, size_z);
  qbModelGroup_* ret;
  qb_modelgroup_create(&ret);
  qbModel model = builder.Model(mode);
  qb_modelgroup_upload(ret, model, nullptr);

  if (collider) {
    *collider = builder.Collider(model->meshes[0]);
  }
  qb_model_destroy(&model);
  return ret;
}

struct qbModelGroup_* qb_draw_rect(float w, float h, qbDrawMode mode, qbCollider* collider) {
  MeshBuilder builder = MeshBuilder::Rect(w, h);
  qbModelGroup_* ret;
  qb_modelgroup_create(&ret);
  qbModel model = builder.Model(mode);
  qb_modelgroup_upload(ret, model, nullptr);

  if (collider) {
    *collider = builder.Collider(model->meshes[0]);
  }
  qb_model_destroy(&model);
  return ret;
}

struct qbModelGroup_* qb_draw_sphere(float radius, int slices, int zslices, qbDrawMode mode, qbCollider* collider) {
  MeshBuilder builder = MeshBuilder::Sphere(radius, slices, zslices);
  qbModelGroup_* ret;
  qb_modelgroup_create(&ret);
  qbModel model = builder.Model(mode);
  qb_modelgroup_upload(ret, model, nullptr);

  if (collider) {
    *collider = builder.Collider(model->meshes[0]);
  }
  qb_model_destroy(&model);
  return ret;
}

void qb_collider_sphere(qbCollider collider, float r) {
  *collider = {};
  collider->max = {  r * 0.5f,  r * 0.5f,  r * 0.5f };
  collider->min = { -r * 0.5f, -r * 0.5f, -r * 0.5f };
  collider->r = r;
  collider->support = [](const qbCollider_* self, const vec3s* dir, const qbTransform_* transform) {
    if (!transform) {
      return glms_vec3_add(transform->position, glms_vec3_scale(*dir, self->r));
    }

    mat4s inv_rot = glms_mat4_inv(transform->orientation);
    vec3s local_dir = glms_vec3(glms_mat4_mulv(inv_rot, glms_vec4(*dir, 1.f)));

    // Returned support in world-space.
    return glms_vec3_add(transform->position, glms_vec3_scale(local_dir, self->r));
  };
}

template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

// http://uu.diva-portal.org/smash/get/diva2:343820/FULLTEXT01.pdf
void qb_collider_aabb(qbCollider collider, vec3s max, vec3s min, vec3s center) {
  *collider = {};
  collider->max = max;
  collider->min = min;
  collider->center = center;
  collider->r = std::max(glms_vec3_norm(glms_vec3_sub(max, center)),
                         glms_vec3_norm(glms_vec3_sub(min, center)));

  collider->support = [](const qbCollider_* self, const vec3s* dir, const qbTransform_* transform) {
    return glms_vec3_add(transform->position, glms_vec3_scale(*dir, self->r));
  };
}

void qb_collider_obb(qbCollider collider, vec3s max, vec3s min, vec3s center) {
  *collider = {};
}

void qb_collider_pill(qbCollider collider, float r, float h) {
  *collider = {};
}

void qb_collider_cylinder(qbCollider collider, float r, float h) {
  *collider = {};
}

void qb_collider_cone(qbCollider collider, float r, float h) {
  *collider = {};
}