#include "mesh_builder.h"
#include "collision_utils.h"

#include <cubez/render.h>
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/extended_min_max.hpp>
#include <GL/glew.h>
#include <algorithm>
#include <map>
#include <set>
#include <fstream>
#include <cstring>
#include <string>
#include <iostream>

struct VertexAttribute {
  glm::vec3 v;
  glm::vec3 vn;
  glm::vec2 vt;

  bool operator<(const VertexAttribute& other) const {
    return memcmp(this, &other, sizeof(VertexAttribute)) > 0;
  }
};

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
    glm::vec3 v = { 0, 0, 0 };
    if (SSCANF(line.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3) {
      builder->AddVertex(std::move(v));
    }
  } else if (token == "vt") {
    glm::vec2 vt = { 0, 0 };
    if (SSCANF(line.c_str(), "%f %f", &vt.x, &vt.y) == 2) {
      builder->AddTexture(std::move(vt));
    }
  } else if (token == "vn") {
    glm::vec3 vn = { 0, 0, 0 };
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
    }
  } else if (token != "#") {
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

glm::vec3 Mesh::Max() const {
  return max_;
}

glm::vec3 Mesh::Min() const {
  return min_;
}

glm::vec3 Mesh::FarthestPoint(const glm::vec3& dir) const {
  float max_dot = 0.0f;
  int ret = 0;

  int index = 0;
  for (const auto& v : v_) {
    float dot = glm::dot(v, dir);
    if (dot > max_dot) {
      max_dot = dot;
      ret = index;
    }
    ++index;
  }

  return v_[ret];
}

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

int MeshBuilder::AddVertex(glm::vec3&& v) {
  v_.push_back(v);
  return v_.size() - 1;
}

int MeshBuilder::AddTexture(glm::vec2&& vt) {
  vt_.push_back(vt);
  return vt_.size() - 1;
}

int MeshBuilder::AddNormal(glm::vec3&& vn) {
  vn_.push_back(vn);
  return vn_.size() - 1;
}

int MeshBuilder::AddFace(Face&& face) {
  f_.push_back(face);
  return f_.size() - 1;
}

int MeshBuilder::AddFace(std::vector<glm::vec3>&& vertices,
                         std::vector<glm::vec2>&& textures,
                         std::vector<glm::vec3>&& normals) {
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
  return f_.size() - 1;
}

MeshBuilder MeshBuilder::Sphere(float radius, int slices, int zslices) {
  MeshBuilder builder;
  float zdir_step = 180.0f / zslices;
  float dir_step = 360.0f / slices;
  for (float zdir = 0; zdir < 180; zdir += zdir_step) {
    for (float dir = 0; dir < 360; dir += dir_step) {
      float zdir_rad_t = glm::radians(zdir);
      float zdir_rad_d = glm::radians(zdir + zdir_step);
      float dir_rad_l = glm::radians(dir);
      float dir_rad_r = glm::radians(dir + dir_step);

      glm::vec3 p0 = radius * glm::vec3(
        sin(zdir_rad_t) * cos(dir_rad_l),
        sin(zdir_rad_t) * sin(dir_rad_l),
        cos(zdir_rad_t)
      );
      glm::vec2 t0 = {
        dir_rad_l / glm::two_pi<float>(),
        zdir_rad_t / glm::pi<float>()
      };

      glm::vec3 p1 = radius * glm::vec3(
        sin(zdir_rad_t) * cos(dir_rad_r),
        sin(zdir_rad_t) * sin(dir_rad_r),
        cos(zdir_rad_t)
      );
      glm::vec2 t1 = {
        dir_rad_r / glm::two_pi<float>(),
        zdir_rad_t / glm::pi<float>()
      };

      glm::vec3 p2 = radius * glm::vec3(
        sin(zdir_rad_d) * cos(dir_rad_r),
        sin(zdir_rad_d) * sin(dir_rad_r),
        cos(zdir_rad_d)
      );
      glm::vec2 t2 = {
        dir_rad_r / glm::two_pi<float>(),
        zdir_rad_d / glm::pi<float>()
      };

      glm::vec3 p3 = radius * glm::vec3(
        sin(zdir_rad_d) * cos(dir_rad_l),
        sin(zdir_rad_d) * sin(dir_rad_l),
        cos(zdir_rad_d)
      );
      glm::vec2 t3 = {
        dir_rad_l / glm::two_pi<float>(),
        zdir_rad_d / glm::pi<float>()
      };

      if (zdir > 0) {
        builder.AddFace({
          p2, p1, p0
        }, {
          t2, t1, t0
        }, {
          p2 / radius, p1 / radius, p0 / radius
        });
      }

      builder.AddFace({
        p2, p0, p3
      }, {
        t2, t0, t3
      }, {
        p2 / radius, p0 / radius, p3 / radius
      });
    }
  }
  return builder;
}

MeshBuilder MeshBuilder::Box(float x, float y, float z) {
  MeshBuilder builder;
  
  glm::vec3 center = 0.5f * glm::vec3{ x, y, z };
  int p1 = builder.AddVertex(glm::vec3{ 0, 0, z } - center);
  int p2 = builder.AddVertex(glm::vec3{ 0, y, z } - center);
  int p3 = builder.AddVertex(glm::vec3{ x, y, z } - center);
  int p4 = builder.AddVertex(glm::vec3{ x, 0, z } - center);
  int p5 = builder.AddVertex(glm::vec3{ 0, 0, 0 } - center);
  int p6 = builder.AddVertex(glm::vec3{ 0, y, 0 } - center);
  int p7 = builder.AddVertex(glm::vec3{ x, y, 0 } - center);
  int p8 = builder.AddVertex(glm::vec3{ x, 0, 0 } - center);

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

  int p1 = builder.AddVertex(glm::vec3{ 0, 0, 0 });
  int p2 = builder.AddVertex(glm::vec3{ 0, y, 0 });
  int p3 = builder.AddVertex(glm::vec3{ x, y, 0 });
  int p4 = builder.AddVertex(glm::vec3{ x, 0, 0 });

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

struct VectorCompare {
  glm::vec3 v;
  bool operator() (const glm::vec3& lhs, const glm::vec3& rhs) const {
    return std::hash<glm::vec3>()(lhs) < std::hash<glm::vec3>()(rhs);
  }
  bool operator< (const VectorCompare& other) const {
    return std::hash<glm::vec3>()(v) < std::hash<glm::vec3>()(other.v);
  }
};

qbCollider MeshBuilder::Collider() {
  qbCollider collider = new qbCollider_;
  
  std::set<glm::vec3, VectorCompare> verts;
  glm::vec3 max(std::numeric_limits<float>::min());
  glm::vec3 min(std::numeric_limits<float>::max());

  for (const glm::vec3& v: v_) {
    max.x = std::max(max.x, v.x);
    max.y = std::max(max.y, v.y);
    max.z = std::max(max.z, v.z);

    min.x = std::min(min.x, v.x);
    min.y = std::min(min.y, v.y);
    min.z = std::min(min.z, v.z);

    verts.insert(v);
   }

  collider->vertices = new glm::vec3[verts.size()];
  collider->count = verts.size();
  size_t i = 0;
  for (auto&& v : verts) {
    collider->vertices[i] = v;
    ++i;
  }

  collider->max = max;
  collider->min = min;
  collider->r.x = std::max(glm::length(max.x), glm::length(min.x));
  collider->r.y = std::max(glm::length(max.y), glm::length(min.y));
  collider->r.z = std::max(glm::length(max.z), glm::length(min.z));

  return collider;
}

struct MatrixCompare {
  glm::mat3 mat;
  bool operator< (const MatrixCompare& other) const {
    return std::hash<glm::mat3>()(mat) < std::hash<glm::mat3>()(other.mat);
  }
};

qbModel MeshBuilder::Model(qbRenderFaceType_ render_mode) {
  std::vector<glm::vec3> vertices;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> uvs;
  std::vector<uint32_t> indices;

  if (render_mode == qbRenderFaceType_::QB_TRIANGLES) {
    std::map<MatrixCompare, uint32_t> mapped_indices;
    for (const Face& face : f_) {
      for (int i = 0; i < 3; ++i) {
        const glm::vec3& v = v_[face.v[i]];
        const glm::vec2& vt = vt_[face.vt[i]];
        const glm::vec3& vn = vn_[face.vn[i]];

        glm::mat3 mat;
        mat[0] = v;
        mat[1] = glm::vec3(vt, 0);
        mat[2] = vn;

        MatrixCompare mc{ mat };

        auto it = mapped_indices.find(mc);
        if (it == mapped_indices.end()) {
          vertices.push_back(v);
          normals.push_back(vn);
          uvs.push_back(vt);

          uint32_t new_index = vertices.size() - 1;
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
        const glm::vec3& v = v_[i];
        VectorCompare vc{ v };

        auto it = mapped_indices.find(vc);
        if (it == mapped_indices.end()) {
          vertices.push_back(v);

          uint32_t new_index = vertices.size() - 1;
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

              uint32_t new_index = vertices.size() - 1;
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
        const glm::vec3& v = v_[i];
        VectorCompare vc{ v };

        auto it = mapped_indices.find(vc);
        if (it == mapped_indices.end()) {
          vertices.push_back(v);

          uint32_t new_index = vertices.size() - 1;
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

            uint32_t new_index = vertices.size() - 1;
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

  qbModel ret = new qbModel_{};
  ret->colliders = nullptr;
  ret->collider_count = 0;
  ret->mesh_count = 1;
  ret->meshes = new qbMesh_[1];
  
  qbMesh mesh = ret->meshes;
  mesh->vertex_count = vertices.size();
  mesh->vertices = new glm::vec3[mesh->vertex_count];
  memcpy(mesh->vertices, vertices.data(), vertices.size() * sizeof(glm::vec3));

  mesh->index_count = indices.size();
  mesh->indices = new uint32_t[mesh->index_count];
  memcpy(mesh->indices, indices.data(), indices.size() * sizeof(uint32_t));

  mesh->normal_count = normals.size();
  mesh->normals = nullptr;
  if (mesh->normal_count > 0) {    
    mesh->normals = new glm::vec3[mesh->normal_count];
    memcpy(mesh->normals, normals.data(), normals.size() * sizeof(glm::vec3));
  }

  mesh->uv_count = uvs.size();
  mesh->uvs = nullptr;
  if (mesh->uv_count > 0) {
    mesh->uvs = new glm::vec2[mesh->uv_count];
    memcpy(mesh->uvs, uvs.data(), uvs.size() * sizeof(glm::vec2));
  }

  return ret;
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
