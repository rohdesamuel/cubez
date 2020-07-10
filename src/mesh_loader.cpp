#include "mesh_loader.h"
#include "tiny_obj_loader.h"

#include <cglm/struct/vec3.h>
#include <filesystem>
#include <iostream>
#include <unordered_map>

struct Vertex {
  vec3s pos;
  vec3s normal;
  vec2s tex;
};

bool operator==(const Vertex& lhs, const Vertex& rhs) {
  return glms_vec3_eqv(lhs.pos, rhs.pos) &&
    glms_vec3_eqv(lhs.normal, rhs.normal) &&
    glm_vec2_eqv((float*)lhs.tex.raw, (float*)rhs.tex.raw);
}

namespace std
{
namespace filesystem = experimental::filesystem;

size_t hash_combine(size_t seed, size_t hash) {
  hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed ^ hash;
}

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
struct hash<vec2s> {
  size_t operator()(const vec2s& v) const {
    size_t seed = 0;
    hash<float> hasher;
    seed = hash_combine(seed, hasher(v.x));
    seed = hash_combine(seed, hasher(v.y));
    return seed;
  }
};

template<> struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const {
    return ((hash<vec3s>()(vertex.pos) ^
            (hash<vec3s>()(vertex.normal) << 1)) >> 1) ^
            (hash<vec2s>()(vertex.tex) << 1);
  }
};
}

qbModel MeshLoader::Load(char** path, const char* filename) {
  std::filesystem::path filepath;

  if (!filename) {
    return nullptr;
  }

  if (!path) {
    filepath = std::filesystem::current_path();
  } else {
    char** d = path;
    while (*d) {
      filepath.append(*d);
      ++d;
    }
  }

  filepath = std::filesystem::path(filepath).append(filename);

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.generic_string().c_str());

  if (!warn.empty()) {
    std::cout << warn << std::endl;
  }

  if (!err.empty()) {
    std::cerr << err << std::endl;
  }

  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); s++) {
    // Loop over faces(polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      int fv = shapes[s].mesh.num_face_vertices[f];

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
        tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
        tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
        tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
        tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
        tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
      }
      index_offset += fv;

      // per-face material
      shapes[s].mesh.material_ids[f];
    }
  }
  return nullptr;
}

qbMesh MeshLoader::ToQbMesh(const tinyobj::shape_t& shape, const tinyobj::attrib_t& attrib) {
  qbMesh ret = new qbMesh_;

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  std::unordered_map<Vertex, uint32_t> unique_vertices;

  indices.reserve(shape.mesh.indices.size());
  vertices.reserve(shape.mesh.indices.size());

  for (auto idx : shape.mesh.indices) {
    Vertex vertex = {};
    
    vertex.pos = {
      attrib.vertices[3 * idx.vertex_index + 0],
      attrib.vertices[3 * idx.vertex_index + 1],
      attrib.vertices[3 * idx.vertex_index + 2]
    };

    vertex.normal = {
      attrib.normals[3 * idx.normal_index + 0],
      attrib.normals[3 * idx.normal_index + 1],
      attrib.normals[3 * idx.normal_index + 2]
    };

    vertex.tex = {
      attrib.texcoords[2 * idx.texcoord_index + 0],
      attrib.texcoords[2 * idx.texcoord_index + 1],
    };

    uint32_t index;
    auto it = unique_vertices.find(vertex);
    if (it == unique_vertices.end()) {
      index = (uint32_t)vertices.size();
      unique_vertices[vertex] = index;
      vertices.push_back(vertex);
    } else {
      index = it->second;
    }

    indices.push_back(index);
  }


  
  return ret;
}

qbMaterial MeshLoader::ToQbMaterial(tinyobj::material_t* material) {
  return nullptr;
}