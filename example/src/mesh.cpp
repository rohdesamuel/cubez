#include "mesh.h"
#include "mesh_builder.h"
#include "render.h"
#include "shader.h"
#include "physics.h"

#include <glm/glm.hpp>
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

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

struct qbCollisionMesh_ {
  glm::vec3 max;
  glm::vec3 min;
  float r;
};

struct qbMaterialTexture_ {
  qbTexture texture;
  glm::vec2 offset;
  glm::vec2 scale;
};

struct qbMaterialAttr_ {
  glm::vec4 color;
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

uint32_t load_texture(const std::string& file) {
  uint32_t texture;

  // Load the image from the file into SDL's surface representation
  SDL_Surface* surf = SDL_LoadBMP(file.c_str());
  if (!surf) {
    std::cout << "Could not load texture " << file << std::endl;
  }
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf->w, surf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surf->pixels);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  SDL_FreeSurface(surf);
  return texture;
}

qbResult process_line(MeshBuilder* builder, const std::string& token, 
                      const std::string& line) {
  if (token == "v") {
    glm::vec3 v = {0, 0, 0};
    if (SSCANF(line.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3) {
      builder->AddVertex(std::move(v));
    }
  } else if (token == "vt") {
    glm::vec2 vt = {0, 0};
    if (SSCANF(line.c_str(), "%f %f", &vt.x, &vt.y) == 2) {
      builder->AddTexture(std::move(vt));
    }
  } else if (token == "vn") {
    glm::vec3 vn = {0, 0, 0};
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
  *mesh = builder.BuildRenderable(qbRenderMode::QB_TRIANGLES);
  return qbResult::QB_OK;
}

qbResult qb_mesh_destroy();

qbResult qb_mesh_draw(qbMesh mesh, qbMaterial) {
  glBindVertexArray(mesh->vao);
  CHECK_GL();
  size_t count = mesh->attributes->indices.size();

  GLenum render_mode = GL_TRIANGLES;
  switch (mesh->render_mode) {
  case QB_POINTS:
    render_mode = GL_POINTS;
    break;
  case QB_LINES:
    render_mode = GL_LINES;
    break;
  case QB_TRIANGLES:
    render_mode = GL_TRIANGLES;
    break;
  }

  glDrawElements(render_mode, (GLsizei)count, GL_UNSIGNED_INT, (void*)0);
  CHECK_GL();
  glBindVertexArray(0);
  return QB_OK;
}

uint32_t qb_mesh_getvbuffer(qbMesh mesh) {
  return mesh->v_vbo;
}

uint32_t qb_mesh_getvtbuffer(qbMesh mesh) {
  return mesh->vt_vbo;
}

uint32_t qb_mesh_getvnbuffer(qbMesh mesh) {
  return mesh->vn_vbo;
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

qbResult qb_materialattr_setcolor(qbMaterialAttr attr, glm::vec4 color) {
  attr->color = color;
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
  (*material)->color = attr->color;

  return QB_OK;
}

qbResult qb_material_destroy(qbMaterial*) {
  return QB_OK;
}

qbResult qb_material_setcolor(qbMaterial material, glm::vec4 color) {
  material->color = color;
  return QB_OK;
}

qbResult qb_material_use(qbMaterial material) {
  ShaderProgram shaders(material->shader->shader_id);
  shaders.use();

  if (material->textures.empty()) {
    glUniform1i(glGetUniformLocation(shaders.id(), "uColorMode"), 1);
    glUniform4f(glGetUniformLocation(shaders.id(), "uDefaultColor"),
                material->color.r,
                material->color.g,
                material->color.b,
                material->color.a);
    CHECK_GL();
  } else {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material->textures[0].texture->texture_id);
    glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
    glUniform1i(glGetUniformLocation(shaders.id(), "uColorMode"), 2);
    CHECK_GL();
  }
  return QB_OK;
}

qbShader qb_material_getshader(qbMaterial material) {
  return material->shader;
}

qbResult qb_shader_load(qbShader* shader, const char*,
    const char* vs_filename, const char* fs_filename, const char* gs_filename) {
  *shader = new qbShader_;
  (*shader)->vs_file = STRDUP(vs_filename);
  (*shader)->fs_file = STRDUP(fs_filename);


  ShaderProgram program = ShaderProgram::load_from_file(vs_filename, fs_filename, gs_filename ? gs_filename : "");
  (*shader)->shader_id = program.id();
  return QB_OK;
}

qbId qb_shader_getid(qbShader shader) {
  return shader->shader_id;
}

qbResult qb_shader_use(qbShader shader) {
  glUseProgram(shader->shader_id);
  return QB_OK;
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
  (*texture)->texture_file = STRDUP(texture_file);
  (*texture)->texture_id = load_texture(texture_file);
  return QB_OK;
}
