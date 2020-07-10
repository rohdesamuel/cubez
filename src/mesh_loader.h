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

#ifndef MESH_LOADER__H
#define MESH_LOADER__H

#include <cubez/mesh.h>
#include "tiny_obj_loader.h"

class MeshLoader {
public:
  qbModel Load(char** directory, const char* filename);

private:
  qbMesh ToQbMesh(const tinyobj::shape_t& shape, const tinyobj::attrib_t& attrib);
  qbMaterial ToQbMaterial(tinyobj::material_t* material);
};

#endif