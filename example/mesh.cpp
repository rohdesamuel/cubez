#include "mesh.h"
#include "render.h"
#include "shader.h"

#include <glm/glm.hpp>
#include <GL/glew.h>

#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

struct VertexAttribute {
  glm::vec3 v;
  glm::vec3 vn;
  glm::vec2 vt;

  bool operator<(const VertexAttribute& other) const {
    return memcmp(this, &other, sizeof(VertexAttribute)) > 0;
  }
};

struct qbMeshAttributes_ {
  std::vector<glm::vec3> v;
  std::vector<glm::vec3> vn;
  std::vector<glm::vec2> vt;
  std::vector<uint32_t> indices;
};

struct qbCollisionMesh_ {
  float r;
};

struct qbMesh_ {
  uint32_t vao;
  uint32_t el_vbo;
  uint32_t v_vbo;
  uint32_t vn_vbo;
  uint32_t vt_vbo;

  qbMeshAttributes_* attributes;
};

struct qbMaterialTexture_ {
  qbTexture texture;
  glm::vec2 offset;
  glm::vec2 scale;
};

struct qbMaterialAttr_ {
  qbShader shader;
  std::vector<qbMaterialTexture_> textures;
};

struct qbMaterial_ {
  glm::vec4 color;

  qbShader shader;
  std::vector<qbMaterialTexture_> textures;
};

struct qbShader_ {
  uint32_t shader_id;
  const char* vs_file;
  const char* fs_file;
};

struct qbTexture_ {
  uint32_t texture_id;
  const char* texture_file;
};

// Zero-based indexing of vertex attributes.
struct Face {
  int v[3];
  int vn[3];
  int vt[3];
};

uint32_t load_texture(const std::string& file) {
  uint32_t texture;

  // Load the image from the file into SDL's surface representation
  SDL_Surface* surf = SDL_LoadBMP(file.c_str());

  if (!surf) {
    std::cout << "Could not load texture " << file << std::endl;
  }
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  SDL_FreeSurface(surf);

  return texture;
}

class MeshBuilder {
 private:
  std::vector<glm::vec3> v_;
  std::vector<glm::vec2> vt_;
  std::vector<glm::vec3> vn_;
  std::vector<Face> f_;

 public:
  void add_vertex(glm::vec3 v) {
    v_.push_back(v);
  }

  void add_texture(glm::vec2 vt) {
    vt_.push_back(vt);
  }

  void add_normal(glm::vec3 vn) {
    vn_.push_back(vn);
  }

  void add_face(Face face) {
    f_.push_back(face);
  }

  qbMesh build() {
    std::cout << "building mesh\n";
    qbMesh mesh = new qbMesh_;
    qbMeshAttributes_* attributes = new qbMeshAttributes_;
    mesh->attributes = attributes;

    std::map<VertexAttribute, uint32_t> mapped_indices;

    for (const Face& face : f_) {
      for (int i = 0; i < 3; ++i) {
        const glm::vec3& v = v_[face.v[i]];
        const glm::vec2& vt = vt_[face.vt[i]];
        const glm::vec3& vn = vn_[face.vn[i]];

        VertexAttribute attr = {v, vn, vt};

        auto it = mapped_indices.find(attr);
        if (it == mapped_indices.end()) {
          attributes->v.push_back(v_[face.v[i]]);
          attributes->vn.push_back(vn_[face.vn[i]]);
          attributes->vt.push_back(vt_[face.vt[i]]);

          uint32_t new_index = attributes->v.size() - 1;
          attributes->indices.push_back(new_index);
          mapped_indices[attr] = new_index;
        } else {
          attributes->indices.push_back(it->second);
        }
      }
    }
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->v_vbo);
    glGenBuffers(1, &mesh->vt_vbo);
    glGenBuffers(1, &mesh->vn_vbo);
    glGenBuffers(1, &mesh->el_vbo);

    std::cout << mesh->vao << ": " << mesh->v_vbo << ", " << mesh->vt_vbo
              << ", " << mesh->vn_vbo << ", " << mesh->el_vbo << "\n";

    glBindVertexArray(mesh->vao);

    // Vertices
    glBindBuffer(GL_ARRAY_BUFFER, mesh->v_vbo);
    glBufferData(GL_ARRAY_BUFFER, attributes->v.size() * sizeof(glm::vec3),
                 attributes->v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,                  // attribute
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void*)0            // array buffer offset
        );

    // UVs
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vt_vbo);
    glBufferData(GL_ARRAY_BUFFER, attributes->vt.size() * sizeof(glm::vec2),
                 attributes->vt.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,                  // attribute
        2,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void*)0            // array buffer offset
        );

    // Normals
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vn_vbo);
    glBufferData(GL_ARRAY_BUFFER, attributes->vn.size() * sizeof(glm::vec3),
                 attributes->vn.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,                  // attribute
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void*)0            // array buffer offset
        );

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->el_vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 attributes->indices.size() * sizeof(uint32_t),
                 attributes->indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return mesh;
  }
};

qbResult process_line(MeshBuilder* builder, const std::string& token,
                                        const std::string& line) {
  if (token == "v") {
    glm::vec3 v = {0, 0, 0};
    if (SSCANF(line.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3) {
      std::cout << "read vertex" << line << std::endl;
      builder->add_vertex(v);
    }
  } else if (token == "vt") {
    glm::vec2 vt = {0, 0};
    if (SSCANF(line.c_str(), "%f %f", &vt.x, &vt.y) == 2) {
      std::cout << "read texture" << line << std::endl;
      builder->add_texture(vt);
    }
  } else if (token == "vn") {
    glm::vec3 vn = {0, 0, 0};
    if (SSCANF(line.c_str(), "%f %f %f", &vn.x, &vn.y, &vn.z) == 3) {
      std::cout << "read normal" << line << std::endl;
      builder->add_normal(vn);
    }
  } else if (token == "f") {
    Face face;
    if (SSCANF(line.c_str(),
                 "%d/%d/%d %d/%d/%d %d/%d/%d",
                 &face.v[0], &face.vt[0], &face.vn[0],
                 &face.v[1], &face.vt[1], &face.vn[1],
                 &face.v[2], &face.vt[2], &face.vn[2]) == 9) {
      std::cout << "read face: " << line << std::endl;
      // Convert from 1-indexing to 0-indexing.
      for (int i = 0; i < 3; ++i) {
        --face.v[i];
        --face.vn[i];
        --face.vt[i];
      }
      builder->add_face(face);
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

qbResult qb_mesh_load(qbMesh* mesh, const char*, const char* filename) {
  std::ifstream file;
  file.open(filename, std::ios::in);
  MeshBuilder builder;
  if (file.is_open()) {
    std::string line;
    while(getline(file, line)) {
      if (process_line(&builder, line) != qbResult::QB_OK) {
        file.close();
        return qbResult::QB_UNKNOWN;
      }
    }
  } else {
    file.close();
    return qbResult::QB_UNKNOWN;
  }
  file.close();
  std::cout << "Done reading mesh file\n";
  *mesh = builder.build();
  return qbResult::QB_OK;
}

qbResult qb_mesh_destroy();

qbResult qb_mesh_draw(qbMesh mesh, qbMaterial) {
  glBindVertexArray(mesh->vao);
  size_t count = mesh->attributes->indices.size();
  glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)0);
  glBindVertexArray(0);
  return QB_OK;
}

qbResult qb_materialattr_create(qbMaterialAttr* attr) {
  *attr = new qbMaterialAttr_;
  return QB_OK;
}

qbResult qb_materialattr_destroy(qbMaterialAttr* attr) {
  delete *attr;
  *attr = nullptr;
  return QB_OK;
}

qbResult qb_materialattr_setshader(qbMaterialAttr attr, qbShader shader) {
  attr->shader = shader;
  return QB_OK;
}

qbResult qb_materialattr_addtexture(qbMaterialAttr attr,
    qbTexture texture, glm::vec2 offset, glm::vec2 scale) {
  attr->textures.push_back({texture, offset, scale});
  return QB_OK;
}

qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr) {
  *material = new qbMaterial_;

  (*material)->shader = attr->shader;
  (*material)->textures = attr->textures;
  
  return QB_OK;
}

qbResult qb_material_destroy(qbMaterial*) {
  return QB_OK;
}

qbResult qb_material_use(qbMaterial material) {
  ShaderProgram shaders(material->shader->shader_id);
  shaders.use();

  if (!material->textures.empty()) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material->textures[0].texture->texture_id);
    glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
  }
  return QB_OK;
}

qbShader qb_material_getshader(qbMaterial material) {
  return material->shader;
}

qbResult qb_shader_load(qbShader* shader, const char*,
    const char* vs_filename, const char* fs_filename) {
  *shader = new qbShader_;
  (*shader)->vs_file = strdup(vs_filename);
  (*shader)->fs_file = strdup(fs_filename);

  ShaderProgram program = ShaderProgram::load_from_file(vs_filename, fs_filename);
  (*shader)->shader_id = program.id();
  return QB_OK;
}

qbId qb_shader_getid(qbShader shader) {
  return shader->shader_id;
}

qbResult qb_shader_setbool(qbShader shader, const char* uniform, bool value) {
  glUniform1i(glGetUniformLocation(shader->shader_id, uniform), (int)value);
  return QB_OK;
}
qbResult qb_shader_setint(qbShader shader, const char* uniform, int value) {
  glUniform1i(glGetUniformLocation(shader->shader_id, uniform), value);
  return QB_OK;
}
qbResult qb_shader_setfloat(qbShader shader, const char* uniform, float value) {
  glUniform1f(glGetUniformLocation(shader->shader_id, uniform), value);
  return QB_OK;
}
qbResult qb_shader_setvec2(qbShader shader, const char* uniform,
                           glm::vec2 value) {
  glUniform2fv(glGetUniformLocation(shader->shader_id, uniform), 1,
                                    (float*)&value);
  return QB_OK;
}
qbResult qb_shader_setvec3(qbShader shader, const char* uniform,
                           glm::vec3 value) {
  glUniform3fv(glGetUniformLocation(shader->shader_id, uniform), 1,
                                    (float*)&value);
  return QB_OK;
}
qbResult qb_shader_setvec4(qbShader shader, const char* uniform,
                           glm::vec4 value) {
  glUniform4fv(glGetUniformLocation(shader->shader_id, uniform), 1,
                                    (float*)&value);
  return QB_OK;
}
qbResult qb_shader_setmat2(qbShader shader, const char* uniform,
                           glm::mat2 value) {
  glUniformMatrix2fv(glGetUniformLocation(shader->shader_id, uniform), 1,
                                          GL_FALSE, (float*)&value);
  return QB_OK;
}
qbResult qb_shader_setmat3(qbShader shader, const char* uniform,
                           glm::mat3 value) {
  glUniformMatrix3fv(glGetUniformLocation(shader->shader_id, uniform), 1,
                                          GL_FALSE, (float*)&value);
  return QB_OK;
}
qbResult qb_shader_setmat4(qbShader shader, const char* uniform,
                           glm::mat4 value) {
  glUniformMatrix4fv(glGetUniformLocation(shader->shader_id, uniform), 1,
                                          GL_FALSE, (float*)&value);
  return QB_OK;
}

qbResult qb_texture_load(qbTexture* texture, const char*,
                         const char* texture_file) {
  *texture = new qbTexture_;
  (*texture)->texture_file = strdup(texture_file);
  (*texture)->texture_id = load_texture(texture_file);
  return QB_OK;
}
