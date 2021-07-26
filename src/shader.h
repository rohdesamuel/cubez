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

#ifndef SHADER__H
#define SHADER__H

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

#include <cubez/cubez.h>

#include <fstream>
#include <iostream>
#include <streambuf>
#include <unordered_map>
#include <vector>
#include <string>

class ShaderProgram {
 public:
  ShaderProgram(const std::string& vs, const std::string& fs, const std::vector<std::string>& extensions);
  ShaderProgram(const std::string& vs, const std::string& fs, const std::string& gs, const std::vector<std::string>& extensions);
  ShaderProgram(GLuint program);

  static ShaderProgram load_from_file(const std::string& vs_file,
                                      const std::string& fs_file,
                                      const std::string& gs_file,
                                      const std::vector<std::string>& extensions);

  void use();
  GLuint id();
  GLuint texture_slot(const std::string& name);

private:
  GLuint create_shader(std::string& shader, GLenum shader_type, const std::vector<std::string>& extensions);
  void register_texture_slots();

  GLuint program_;
  
  std::unordered_map<std::string, GLuint> sampler_name_to_texture_slot_;
};

#endif  // SHADER__H
