#include "mesh.h"

#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <string>
#include <tuple>
#include <vector>

struct VertexAttribute {
  glm::vec3 v;
  glm::vec3 vn;
  glm::vec2 vt;
};

struct qbMesh_ {
  VertexAttribute* vertices;
  size_t count;
};

class MeshBuilder {
 private:
  std::vector<glm::vec4> v_;
  std::vector<std::vector<std::tuple<int, int, int>>> f_;
  std::vector<glm::vec3> vt_;
  std::vector<glm::vec3> vn_;

 public:
  void add_vertex(glm::vec4 v) {
    v_.push_back(v);
  }

  void add_texture(glm::vec3 vt) {
    vt_.push_back(vt);
  }

  void add_normal(glm::vec3 vn) {
    vn_.push_back(vn);
  }

  void add_face(std::vector<std::tuple<int, int, int>> indices) {
    f_.push_back(indices);
  }

  qbMesh build() {
    qbMesh mesh = (qbMesh)malloc(sizeof(qbMesh_));
    mesh->count = v_.size();
    mesh->vertices = (VertexAttribute*)calloc(mesh->count,
                                              sizeof(VertexAttribute));
    for (size_t i = 0; i < mesh->count; ++i) {

    }
    return mesh;
  }
};

std::vector<std::string> split(const std::string& line, char delimiter) {
  std::vector<std::string> args;
  size_t last = 0;
  size_t next = 0;
  while((next = line.find(delimiter, last)) != std::string::npos) {
    args.push_back(line.substr(last, next - last));
    last = next + 1;
  }

  return args;
}

void process_line(MeshBuilder* builder, const std::string& token,
                                        const std::vector<std::string>& args) {
  if (token == "v") {
    glm::vec4 v = {0, 0, 0, 1};
    v.x = ::atof(args[0].c_str());
    v.y = ::atof(args[1].c_str());
    v.z = ::atof(args[2].c_str());

    // 1 + 4 coordinates
    if (args.size() == 4) {
      v.w = ::atof(args[3].c_str());
    }
    builder->add_vertex(v);
  } else if (token == "vt") {
    glm::vec3 vt = {0, 0, 0};
    vt.x = ::atof(args[0].c_str());
    vt.y = ::atof(args[1].c_str());

    if (args.size() == 3) {
      vt.z = ::atof(args[2].c_str());
    }
    builder->add_texture(vt);
  } else if (token == "vn") {
    glm::vec3 vn = {0, 0, 0};
    vn.x = ::atof(args[0].c_str());
    vn.y = ::atof(args[1].c_str());
    vn.z = ::atof(args[2].c_str());
    builder->add_normal(vn);
  } else if (token == "f") {
    if (args.size() == 0) {
      return;
    }

    // Skip the token.
    std::vector<std::tuple<int, int, int>> face;
    for (const std::string& arg : args) {
      std::vector<std::string> v = split(arg, '/');
      int f[3] = {0};
      for (int i = 0; i < 3; ++i) {
        std::string attr = v.at(i);
        if (!attr.empty()) {
          f[i] = ::atoi(attr.c_str());
        }
      }
      face.emplace_back(f[0], f[1], f[2]);
    }
    builder->add_face(face);
  } else if (token != "#") {
    // Bad format
  }
}

void process_line(MeshBuilder* builder, const std::string& line) {
  if (line.empty() || line == "\n") {
    return;
  }
  std::vector<std::string> args = split(line, ' ');
  if (args.empty()) {
    return;
  }

  std::string token = args[0];
  args.erase(args.begin());
  process_line(builder, token, args);
}

qbResult qb_mesh_load(qbMesh* mesh, const char* filename) {
  std::ifstream file;
  file.open(filename, std::ios::in);
  MeshBuilder builder;
  if (file.is_open()) {
    std::string line;
    while(getline(file, line)) {
      process_line(&builder, line);
    }
  } else {
    return qbResult::QB_UNKNOWN;
  }
  *mesh = builder.build();
  return qbResult::QB_OK;
}
qbResult qb_mesh_destroy();
