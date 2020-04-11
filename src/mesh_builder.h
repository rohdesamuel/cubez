#ifndef MESH_BUILDER__H
#define MESH_BUILDER__H

#include <cubez/mesh.h>

#include <array>
#include <bitset>
#include <vector>

#include <cglm/types-struct.h>

typedef struct {
  vec3s orig;
  vec3s dir;
} qbRay_, *qbRay;

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
  static MeshBuilder Rect(float x, float y);

  int AddVertex(vec3s v);
  int AddVertexWithOffset(vec3s v, vec3s center);
  int AddTexture(vec2s vt);
  int AddNormal(vec3s vn);
  int AddFace(Face face);
  int AddFace(std::vector<vec3s>&& vertices,
              std::vector<vec2s>&& textures,
              std::vector<vec3s>&& normals);

  qbCollider Collider();
  qbModel Model(qbRenderFaceType_ render_mode);

  void Reset();

private:
  uint32_t FaceToTriangles(const Face& face,
                           std::vector<vec3>* v_list,
                           std::vector<vec3>* vn_list,
                           std::vector<vec2>* vt_list);
  uint32_t FaceToLines(const Face& face,
                       std::vector<vec3>* v_list,
                       std::vector<vec3>* vn_list,
                       std::vector<vec2>* vt_list);
  uint32_t FaceToPoints(const Face& face,
                        std::vector<vec3>* v_list,
                        std::vector<vec3>* vn_list,
                        std::vector<vec2>* vt_list);
  std::vector<vec3s> v_;
  std::vector<vec2s> vt_;
  std::vector<vec3s> vn_;
  std::vector<Face> f_;
};

#endif  // MESH_BUILDER__H

