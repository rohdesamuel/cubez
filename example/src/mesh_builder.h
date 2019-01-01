#ifndef MESH_BUILDER__H
#define MESH_BUILDER__H

#include "mesh.h"

#include <array>
#include <bitset>

class Ray {
public:
  Ray(glm::vec3&& origin, glm::vec3&& dir)
    : orig(origin), dir(dir) {}

  glm::vec3 orig;
  glm::vec3 dir;
};

class Mesh {
public:
  glm::vec3 FarthestPoint(const glm::vec3& dir) const;
  glm::vec3 Max() const;
  glm::vec3 Min() const;

private:
  std::vector<glm::vec3> v_;

  glm::vec3 max_;
  glm::vec3 min_;
  glm::vec3 r_;

  friend class MeshBuilder;
};

class Model {
public:
  static Model FromFile(const std::string& filename);
  bool CollidesWith(const glm::vec3& a_origin, const glm::vec3& b_origin,
                    const Model& a, const Model& b);

private:
  std::vector<Mesh> meshes_;
  std::vector<qbMesh> render_meshes_;
  std::vector<Mesh> collision_meshes_;
};

class MeshBuilder {
public:
  // Zero-based indexing of vertex attributes.
  struct Face {
    int v[3];
    int vn[3];
    int vt[3];
  };

  static MeshBuilder FromFile(const std::string& filename);
  static MeshBuilder Sphere(float radius, int slices, int zslices);
  static MeshBuilder Box(float x, float y, float z);


  int AddVertex(glm::vec3&& v);

  int AddTexture(glm::vec2&& vt);

  int AddNormal(glm::vec3&& vn);

  int AddFace(Face&& face);

  int AddFace(std::vector<glm::vec3>&& vertices,
              std::vector<glm::vec2>&& textures,
              std::vector<glm::vec3>&& normals);

  Mesh BuildMesh();
  qbMesh BuildRenderable(qbRenderMode render_mode);

  void Reset();

private:
  uint32_t FaceToTriangles(const Face& face,
                           std::vector<glm::vec3>* v_list,
                           std::vector<glm::vec3>* vn_list,
                           std::vector<glm::vec2>* vt_list);
  uint32_t FaceToLines(const Face& face,
                       std::vector<glm::vec3>* v_list,
                       std::vector<glm::vec3>* vn_list,
                       std::vector<glm::vec2>* vt_list);
  uint32_t FaceToPoints(const Face& face,
                        std::vector<glm::vec3>* v_list,
                        std::vector<glm::vec3>* vn_list,
                        std::vector<glm::vec2>* vt_list);
  std::vector<glm::vec3> v_;
  std::vector<glm::vec2> vt_;
  std::vector<glm::vec3> vn_;
  std::vector<Face> f_;

};

#endif  // MESH_BUILDER__H

