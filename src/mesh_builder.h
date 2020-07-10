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

#ifndef MESH_BUILDER__H
#define MESH_BUILDER__H

#include <cubez/mesh.h>

#include <array>
#include <bitset>
#include <vector>

#include <cglm/types-struct.h>

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

  qbCollider Collider(qbMesh mesh);
  qbModel Model(qbRenderFaceType_ render_mode);

  void Reset();

private:
  
  std::vector<vec3s> v_;
  std::vector<vec2s> vt_;
  std::vector<vec3s> vn_;
  std::vector<Face> f_;
};

#endif  // MESH_BUILDER__H

