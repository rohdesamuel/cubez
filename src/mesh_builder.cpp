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

#include "mesh_builder.h"
#include "collision_utils.h"
#include "quickhull.h"

#include <cubez/render.h>

#include <GL/glew.h>
#include <algorithm>
#include <map>
#include <set>
#include <fstream>
#include <cstring>
#include <string>
#include <iostream>
#include <cglm/struct/vec3.h>
#include <unordered_set>

#include "meshoptimizer/meshoptimizer.h"

size_t hash_combine(size_t seed, size_t hash) {
  hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed ^ hash;
}

namespace std {
template<>
struct hash<vec3s> {
  size_t operator()(const vec3s& v) const {
    size_t seed = 0;
    hash<float> hasher;
    seed = hash_combine(seed, hasher(v.x));
    seed = hash_combine(seed, hasher(v.y));
    seed = hash_combine(seed, hasher(v.z));
    return seed;
  }
};

template<>
struct hash<mat3s> {
  size_t operator()(const mat3s& m) const {
    size_t seed = 0;
    hash<vec3s> hasher;
    seed = hash_combine(seed, hasher(m.col[0]));
    seed = hash_combine(seed, hasher(m.col[1]));
    seed = hash_combine(seed, hasher(m.col[2]));
    return seed;
  }
};
}

struct VectorCompare {
  vec3s v;
  bool operator() (const vec3s& lhs, const vec3s& rhs) const {
    return std::hash<vec3s>()(lhs) < std::hash<vec3s>()(rhs);
  }
  bool operator< (const VectorCompare& other) const {
    return std::hash<vec3s>()(v) < std::hash<vec3s>()(other.v);
  }
  bool operator== (const VectorCompare& other) const {
    return std::hash<vec3s>()(v) == std::hash<vec3s>()(other.v);
  }
};

struct MatrixCompare {
  mat3s mat;
  bool operator< (const MatrixCompare& other) const {
    return std::hash<mat3s>()(mat) < std::hash<mat3s>()(other.mat);
  }
};

namespace std
{
template<>
struct hash<VectorCompare> {
  size_t operator()(const VectorCompare& v) const {
    return std::hash<vec3s>()(v.v);
  }
};
}

namespace {
bool check_for_gl_errors() {
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    return false;
  }
  while (error != GL_NO_ERROR) {
    const GLubyte* error_str = gluErrorString(error);
    std::cout << "Error(" << error << "): " << error_str << std::endl;
    error = glGetError();
  }
  return true;
}

qbResult process_line(MeshBuilder* builder, const std::string& token,
                      const std::string& line) {
  if (token == "v") {
    vec3s v = { 0, 0, 0 };
    if (SSCANF(line.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3) {
      builder->AddVertex(std::move(v));
    }
  } else if (token == "vt") {
    vec2s vt = { 0, 0 };
    if (SSCANF(line.c_str(), "%f %f", &vt.x, &vt.y) == 2) {
      builder->AddTexture(std::move(vt));
    }
  } else if (token == "vn") {
    vec3s vn = { 0, 0, 0 };
    if (SSCANF(line.c_str(), "%f %f %f", &vn.x, &vn.y, &vn.z) == 3) {
      builder->AddNormal(std::move(vn));
    }
  } else if (token == "f") {
    MeshBuilder::Face face;
    if (SSCANF(line.c_str(),
               "%d/%d/%d %d/%d/%d %d/%d/%d",
               &face.v[0], &face.vt[0], &face.vn[0],
               &face.v[1], &face.vt[1], &face.vn[1],
               &face.v[2], &face.vt[2], &face.vn[2]) == 9) {
      // Convert from 1-indexing to 0-indexing.
      for (int i = 0; i < 3; ++i) {
        --face.v[i];
        --face.vn[i];
        --face.vt[i];
      }
      builder->AddFace(std::move(face));
    } else if (SSCANF(line.c_str(),
                      "%d//%d %d//%d %d//%d",
                      &face.v[0], &face.vn[0],
                      &face.v[1], &face.vn[1],
                      &face.v[2], &face.vn[2]) == 6) {
      // Convert from 1-indexing to 0-indexing.
      for (int i = 0; i < 3; ++i) {
        --face.v[i];
        --face.vn[i];
      }
      face.vt[0] = face.vt[1] = face.vt[2] = -1;
      builder->AddFace(std::move(face));
    } else if (SSCANF(line.c_str(),
                      "%d/%d %d/%d %d/%d",
                       &face.v[0], &face.vt[0],
                       &face.v[1], &face.vt[1],
                       &face.v[2], &face.vt[2]) == 6) {
      // Convert from 1-indexing to 0-indexing.
      for (int i = 0; i < 3; ++i) {
        --face.v[i];
        --face.vt[i];
      }
      face.vn[0] = face.vn[1] = face.vn[2] = -1;
      builder->AddFace(std::move(face));
    }
  } else if (token != "#" && token != "mtllib" && token != "o") {
    // Bad format
    return QB_UNKNOWN;
  }
  return QB_OK;
}

qbResult process_line(MeshBuilder* builder, const std::string& line) {
  if (line.empty() || line == "\n") {
    return QB_OK;
  }
  size_t token_end = line.find(' ');
  if (token_end == std::string::npos) {
    return QB_UNKNOWN;
  }
  std::string token = line.substr(0, token_end);

  return process_line(builder, token, line.substr(token_end, line.size() - token_end));
}

}

struct qbMeshBuilder_ {
  MeshBuilder builder;
};

MeshBuilder MeshBuilder::FromFile(const std::string& filename) {
  std::ifstream file;
  file.open(filename, std::ios::in);
  MeshBuilder builder;
  if (file.is_open()) {
    std::string line;
    while (getline(file, line)) {
      if (process_line(&builder, line) != qbResult::QB_OK) {
        goto cleanup;
      }
    }
  }

cleanup:
  file.close();
  return builder;
}

int MeshBuilder::AddVertex(vec3s v) {
  v_.push_back(v);
  return (int)v_.size() - 1;
}

int MeshBuilder::AddVertexWithOffset(vec3s v, vec3s center) {
  center = glms_vec3_negate(center);
  v = glms_vec3_add(v, center);
  v_.push_back(v);
  return (int)v_.size() - 1;
}

int MeshBuilder::AddTexture(vec2s vt) {
  vt_.push_back(vt);
  return (int)vt_.size() - 1;
}

int MeshBuilder::AddNormal(vec3s vn) {
  vn_.push_back(vn);
  return (int)vn_.size() - 1;
}

int MeshBuilder::AddFace(Face face) {
  f_.push_back(face);
  return (int)f_.size() - 1;
}

int MeshBuilder::AddFace(std::vector<vec3s>&& vertices,
                         std::vector<vec2s>&& textures,
                         std::vector<vec3s>&& normals) {
  Face f;
  for (size_t i = 0; i < vertices.size(); ++i) {
    f.v[i] = AddVertex(std::move(vertices[i]));
  }
  for (size_t i = 0; i < textures.size(); ++i) {
    f.vt[i] = AddTexture(std::move(textures[i]));
  }
  for (size_t i = 0; i < normals.size(); ++i) {
    f.vn[i] = AddNormal(std::move(normals[i]));
  }
  f_.push_back(std::move(f));
  return (int)f_.size() - 1;
}

int MeshBuilder::AddFace(int vertices[], int normals[], int uvs[]) {
  Face f = {};
  memcpy(f.v, vertices, sizeof(int) * 3);

  if (normals) {
    memcpy(f.vn, normals, sizeof(int) * 3);
  } else {
    f.vn[0] = f.vn[1] = f.vn[2] = -1;
  }

  if (uvs) {
    memcpy(f.vt, uvs, sizeof(int) * 3);
  } else {
    f.vt[0] = f.vt[1] = f.vt[2] = -1;
  }

  f_.push_back(std::move(f));
  return (int)f_.size() - 1;
}

MeshBuilder MeshBuilder::Sphere(float radius, int slices, int zslices) {
  MeshBuilder builder;
  float zdir_step = 180.0f / zslices;
  float dir_step = 360.0f / slices;
  for (float zdir = 0; zdir < 180; zdir += zdir_step) {
    for (float dir = 0; dir < 360; dir += dir_step) {
      float zdir_rad_t = glm_rad(zdir);
      float zdir_rad_d = glm_rad(zdir + zdir_step);
      float dir_rad_l = glm_rad(dir);
      float dir_rad_r = glm_rad(dir + dir_step);

      vec3s p0 = {
        sin(zdir_rad_t) * cos(dir_rad_l),
        sin(zdir_rad_t) * sin(dir_rad_l),
        cos(zdir_rad_t)
      };      
      vec2s t0 = {
        dir_rad_l / (2.0f * GLM_PIf),
        zdir_rad_t / GLM_PIf
      };
      p0 = glms_vec3_scale(p0, radius);

      vec3s p1 = {
        sin(zdir_rad_t) * cos(dir_rad_r),
        sin(zdir_rad_t) * sin(dir_rad_r),
        cos(zdir_rad_t)
      };
      vec2s t1 = {
        dir_rad_r / (2.0f * GLM_PIf),
        zdir_rad_t / GLM_PIf
      };
      p1 = glms_vec3_scale(p1, radius);

      vec3s p2 = {
        sin(zdir_rad_d) * cos(dir_rad_r),
        sin(zdir_rad_d) * sin(dir_rad_r),
        cos(zdir_rad_d)
      };
      vec2s t2 = {
        dir_rad_r / (2.0f * GLM_PIf),
        zdir_rad_d / GLM_PIf
      };
      p2 = glms_vec3_scale(p2, radius);

      vec3s p3 = {
        sin(zdir_rad_d) * cos(dir_rad_l),
        sin(zdir_rad_d) * sin(dir_rad_l),
        cos(zdir_rad_d)
      };
      vec2s t3 = {
        dir_rad_l / (2.0f * GLM_PIf),
        zdir_rad_d / GLM_PIf
      };
      p3 = glms_vec3_scale(p3, radius);

      if (zdir > 0) {
        builder.AddFace({
          p2, p1, p0
        }, {
          t2, t1, t0
        }, {
          glms_vec3_scale(p2, 1.0f / radius),
          glms_vec3_scale(p1, 1.0f / radius),
          glms_vec3_scale(p0, 1.0f / radius),
        });
      }

      builder.AddFace({
        p2, p0, p3
      }, {
        t2, t0, t3
      }, {
        glms_vec3_scale(p2, 1.0f / radius),
        glms_vec3_scale(p0, 1.0f / radius),
        glms_vec3_scale(p3, 1.0f / radius),
      });
    }
  }
  return builder;
}

MeshBuilder MeshBuilder::Box(float x, float y, float z) {
  MeshBuilder builder;
  
  vec3s center = glms_vec3_scale(vec3s{ x, y, z }, 0.5f);
  int p1 = builder.AddVertexWithOffset(vec3s{ 0, 0, z }, center);
  int p2 = builder.AddVertexWithOffset(vec3s{ 0, y, z }, center);
  int p3 = builder.AddVertexWithOffset(vec3s{ x, y, z }, center);
  int p4 = builder.AddVertexWithOffset(vec3s{ x, 0, z }, center);
  int p5 = builder.AddVertexWithOffset(vec3s{ 0, 0, 0 }, center);
  int p6 = builder.AddVertexWithOffset(vec3s{ 0, y, 0 }, center);
  int p7 = builder.AddVertexWithOffset(vec3s{ x, y, 0 }, center);
  int p8 = builder.AddVertexWithOffset(vec3s{ x, 0, 0 }, center);

  int t1 = builder.AddTexture({ 0, 0 });
  int t2 = builder.AddTexture({ 1, 0 });
  int t3 = builder.AddTexture({ 1, 1 });
  int t4 = builder.AddTexture({ 0, 1 });
  
  // Top
  int n1 = builder.AddNormal({  0,  0,  1 });

  // Bottom
  int n2 = builder.AddNormal({  0,  0, -1 });

  // Front
  int n3 = builder.AddNormal({  1,  0,  0 });

  // Back
  int n4 = builder.AddNormal({ -1,  0,  0 });

  // Left
  int n5 = builder.AddNormal({  0, -1,  0 });

  // Right
  int n6 = builder.AddNormal({  0,  1,  0 });

  // Top
  builder.AddFace({
    { p3, p2, p1 },
    { n1, n1, n1 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p1, p4, p3 },
    { n1, n1, n1 },
    { t3, t4, t1 },
  });

  // Bottom
  builder.AddFace({
    { p5, p6, p7 },
    { n2, n2, n2 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p7, p8, p5 },
    { n2, n2, n2 },
    { t3, t4, t1 },
  });

  // Front
  builder.AddFace({
    { p7, p3, p4 },
    { n3, n3, n3 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p4, p8, p7 },
    { n3, n3, n3 },
    { t3, t4, t1 },
  });

  // Back
  builder.AddFace({
    { p5, p1, p2 },
    { n4, n4, n4 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p2, p6, p5 },
    { n4, n4, n4 },
    { t3, t4, t1 },
  });

  // Left
  builder.AddFace({
    { p8, p4, p1 },
    { n5, n5, n5 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p1, p5, p8 },
    { n5, n5, n5 },
    { t3, t4, t1 },
  });

  // Right
  builder.AddFace({
    { p6, p2, p3 },
    { n6, n6, n6 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p3, p7, p6 },
    { n6, n6, n6 },
    { t3, t4, t1 },
  });
   
  return builder;
}

MeshBuilder MeshBuilder::Rect(float x, float y) {
  MeshBuilder builder;

  int p1 = builder.AddVertex(vec3s{ 0, 0, 0 });
  int p2 = builder.AddVertex(vec3s{ 0, y, 0 });
  int p3 = builder.AddVertex(vec3s{ x, y, 0 });
  int p4 = builder.AddVertex(vec3s{ x, 0, 0 });

  int t1 = builder.AddTexture({ 0, 0 });
  int t2 = builder.AddTexture({ 1, 0 });
  int t3 = builder.AddTexture({ 1, 1 });
  int t4 = builder.AddTexture({ 0, 1 });

  int n1 = builder.AddNormal({ 0, 0, 1 });

  // Top
  builder.AddFace({
    { p3, p2, p1 },
    { n1, n1, n1 },
    { t1, t2, t3 },
  });
  builder.AddFace({
    { p1, p4, p3 },
    { n1, n1, n1 },
    { t3, t4, t1 },
  });

  return builder;
}

qbCollider MeshBuilder::Collider(qbMesh mesh) {
  qbCollider collider = new qbCollider_;

  std::unordered_set<VectorCompare> vertex_deduper;
  for (size_t i = 0; i < mesh->vertex_count; ++i) {
    vertex_deduper.insert(VectorCompare{ mesh->vertices[i] });
  }

  std::vector<vec3s> unique_vertices;
  unique_vertices.reserve(vertex_deduper.size());
  for (auto& v : vertex_deduper) {
    unique_vertices.push_back(v.v);
  }

  {
    std::unordered_set<VectorCompare> empty_dededuper;
    std::swap(vertex_deduper, empty_dededuper);
  }

  qh_mesh_t c_mesh = qh_quickhull3d((qh_vertex_t*)unique_vertices.data(), (unsigned int)unique_vertices.size());

  const size_t target_vertex_count = 255;

  if (c_mesh.nvertices > target_vertex_count) {
    std::vector<uint32_t> indices(target_vertex_count);
    size_t vertex_count = meshopt_simplifyPoints(indices.data(),
      (float*)c_mesh.vertices, c_mesh.nvertices, sizeof(vec3s), target_vertex_count);

    assert(vertex_count <= target_vertex_count && "Could not simplify mesh below 255 vertices.");

    std::vector<vec3s> vertices(c_mesh.nvertices);
    vertex_count = meshopt_optimizeVertexFetch(
      vertices.data(), indices.data(), indices.size(),
      (float*)c_mesh.vertices, c_mesh.nvertices, sizeof(vec3s));
    
    vertices.resize(vertex_count);

    collider->vertex_count = (uint8_t)vertex_count;
    collider->vertices = (vec3s*)calloc(collider->vertex_count, sizeof(vec3s));
    memcpy(collider->vertices, vertices.data(), sizeof(vec3s) * vertex_count);    
  } else {
    collider->vertex_count = (uint8_t)c_mesh.nvertices;
    collider->vertices = (vec3s*)calloc(collider->vertex_count, sizeof(vec3s));
    memcpy(collider->vertices, c_mesh.vertices, sizeof(vec3s) * c_mesh.nvertices);
  }

  qh_free_mesh(c_mesh);
  
  vec3s max, min, mean;
  max = glms_vec3_fill(std::numeric_limits<float>::lowest());
  min = glms_vec3_fill(std::numeric_limits<float>::max());
  mean = GLMS_VEC3_ZERO_INIT;

  for (size_t i = 0; i < collider->vertex_count; ++i) {
    vec3s* v = (vec3s*)(collider->vertices + i);
    max = glms_vec3_maxv(max, *v);
    min = glms_vec3_minv(min, *v);
    mean = glms_vec3_add(mean, *v);
  }
  
  float r = std::max(glms_vec3_norm(max), glms_vec3_norm(min));

  collider->max = { r, r, r };
  collider->min = { -r, -r, -r };
  collider->center = glms_vec3_divs(mean, collider->vertex_count);
  collider->r = r;

  return collider;
}

qbModel MeshBuilder::Model(qbRenderFaceType_ render_mode) {
  qbMesh mesh = Mesh(render_mode);

  if (!mesh) {
    return nullptr;
  }

  qbModel ret = new qbModel_{};
  ret->mesh_count = 1;
  ret->meshes = mesh;
  ret->colliders = Collider(mesh);
  ret->collider_count = 1;

  return ret;
}

qbMesh MeshBuilder::Mesh(qbRenderFaceType_ render_mode) {
  std::vector<vec3s> vertices;
  std::vector<vec3s> normals;
  std::vector<vec2s> uvs;
  std::vector<uint32_t> indices;

  if (render_mode == qbRenderFaceType_::QB_TRIANGLES) {
    std::map<MatrixCompare, uint32_t> mapped_indices;
    for (const Face& face : f_) {
      for (int i = 0; i < 3; ++i) {
        const vec3s& v = v_[face.v[i]];
        vec2s vt = {};
        vec3s vn = {};
        if (face.vt[i] >= 0) {
          vt = vt_[face.vt[i]];
        }

        if (face.vn[i] >= 0) {
          vn = vn_[face.vn[i]];
        }

        mat3s mat;
        mat.col[0] = v;
        mat.col[1] = vec3s{ vt.x, vt.y, 0.0f };
        mat.col[2] = vn;

        MatrixCompare mc{ mat };

        auto it = mapped_indices.find(mc);
        if (it == mapped_indices.end()) {
          if (face.vt[i] >= 0) {
            uvs.push_back(vt);
          }
          if (face.vn[i] >= 0) {
            normals.push_back(vn);
          }
          vertices.push_back(v);          

          uint32_t new_index = (uint32_t)vertices.size() - 1;
          indices.push_back(new_index);
          mapped_indices[mc] = new_index;
        } else {
          indices.push_back(it->second);
        }
      }
    }
  } else if (render_mode == qbRenderFaceType_::QB_LINES) {
    std::map<VectorCompare, uint32_t> mapped_indices;
    if (f_.empty()) {
      for (size_t i = 0; i < v_.size(); ++i) {
        const vec3s& v = v_[i];
        VectorCompare vc{ v };

        auto it = mapped_indices.find(vc);
        if (it == mapped_indices.end()) {
          vertices.push_back(v);

          uint32_t new_index = (uint32_t)vertices.size() - 1;
          indices.push_back(new_index);
          mapped_indices[vc] = new_index;
        } else {
          indices.push_back(it->second);
        }
      }
    } else {
      for (const Face& face : f_) {
        for (int i = 0; i < 3; ++i) {
          for (int j = 0; j < 2; ++j) {
            int index = (i + j) % 3;
            VectorCompare vc{ v_[face.v[index]] };

            auto it = mapped_indices.find(vc);
            if (it == mapped_indices.end()) {
              vertices.push_back(v_[face.v[index]]);
              normals.push_back(vn_[face.vn[index]]);
              uvs.push_back(vt_[face.vt[index]]);

              uint32_t new_index = (uint32_t)vertices.size() - 1;
              indices.push_back(new_index);
              mapped_indices[vc] = new_index;
            } else {
              indices.push_back(it->second);
            }
          }
        }
      }
    }
  } else if (render_mode == qbRenderFaceType_::QB_POINTS) {
    std::map<VectorCompare, uint32_t> mapped_indices;
    if (f_.empty()) {
      for (size_t i = 0; i < v_.size(); ++i) {
        const vec3s& v = v_[i];
        VectorCompare vc{ v };

        auto it = mapped_indices.find(vc);
        if (it == mapped_indices.end()) {
          vertices.push_back(v);

          uint32_t new_index = (uint32_t)vertices.size() - 1;
          indices.push_back(new_index);
          mapped_indices[vc] = new_index;
        } else {
          indices.push_back(it->second);
        }
      }
    } else {
      for (const Face& face : f_) {
        for (int i = 0; i < 3; ++i) {
          VectorCompare vc{ v_[face.v[i]] };

          auto it = mapped_indices.find(vc);
          if (it == mapped_indices.end()) {
            vertices.push_back(v_[face.v[i]]);
            normals.push_back(vn_[face.vn[i]]);
            uvs.push_back(vt_[face.vt[i]]);

            uint32_t new_index = (uint32_t)vertices.size() - 1;
            indices.push_back(new_index);
            mapped_indices[vc] = new_index;
          } else {
            indices.push_back(it->second);
          }
        }
      }
    }
  }

  if (indices.size() == 0 || vertices.size() == 0) {
    return nullptr;
  }

  assert(vertices.size() < (uint32_t)(0xFFFFFFFF) && "Mesh exceeds max vertex count.");
  assert(indices.size() < (uint32_t)(0xFFFFFFFF) && "Mesh exceeds max index count.");
  assert(normals.size() < (uint32_t)(0xFFFFFFFF) && "Mesh exceeds max normal count.");
  assert(uvs.size() < (uint32_t)(0xFFFFFFFF) && "Mesh exceeds max uv count.");

  qbMesh mesh = new qbMesh_;
  mesh->vertex_count = (uint32_t)vertices.size();
  mesh->vertices = new vec3s[mesh->vertex_count];
  memcpy(mesh->vertices, vertices.data(), mesh->vertex_count * sizeof(vec3));

  mesh->index_count = (uint32_t)indices.size();
  mesh->indices = new uint32_t[mesh->index_count];
  memcpy(mesh->indices, indices.data(), mesh->index_count * sizeof(uint32_t));

  mesh->normal_count = (uint32_t)normals.size();
  mesh->normals = nullptr;
  if (mesh->normal_count > 0) {    
    mesh->normals = new vec3s[mesh->normal_count];
    memcpy(mesh->normals, normals.data(), mesh->normal_count * sizeof(vec3));
  }

  mesh->uv_count = (uint32_t)uvs.size();
  mesh->uvs = nullptr;
  if (mesh->uv_count > 0) {
    mesh->uvs = new vec2s[mesh->uv_count];
    memcpy(mesh->uvs, uvs.data(), mesh->uv_count * sizeof(vec2));
  }

  return mesh;
}

void MeshBuilder::Reset() {
  decltype(v_) empty_v;
  decltype(vt_) empty_vt;
  decltype(vn_) empty_vn;
  decltype(f_) empty_f;
  
  std::swap(v_, empty_v);
  std::swap(vt_, empty_vt);
  std::swap(vn_, empty_vn);
  std::swap(f_, empty_f);
}

qbResult qb_meshbuilder_create(qbMeshBuilder* builder) {
  *builder = new qbMeshBuilder_{};
  return QB_OK;
}

qbResult qb_meshbuilder_destroy(qbMeshBuilder* builder) {
  delete *builder;
  return QB_OK;
}

qbResult qb_meshbuilder_build(qbMeshBuilder builder, qbMesh* mesh, qbCollider* collider) {
  if (mesh) {
    *mesh = builder->builder.Mesh(qbRenderFaceType_::QB_TRIANGLES);
  }

  if (collider) {
    *collider = builder->builder.Collider(*mesh);
  }
  return QB_OK;
}

int qb_meshbuilder_addv(qbMeshBuilder builder, vec3s v) {
  return builder->builder.AddVertex(v);
}

int qb_meshbuilder_addvo(qbMeshBuilder builder, vec3s v, vec3s o) {
  return builder->builder.AddVertexWithOffset(v, o);
}

int qb_meshbuilder_addvt(qbMeshBuilder builder, vec2s vt) {
  return builder->builder.AddTexture(vt);
}

int qb_meshbuilder_addvn(qbMeshBuilder builder, vec3s vn) {
  return builder->builder.AddNormal(vn);
}

int qb_meshbuilder_addtri(qbMeshBuilder builder, int vertices[], int normals[], int uvs[]) {
  return builder->builder.AddFace(vertices, normals, uvs);
}
