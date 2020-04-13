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

class ShaderProgram {
 private:
  GLuint create_shader(const char* shader, GLenum shader_type);

  GLuint program_;

 public:
  ShaderProgram(const std::string& vs, const std::string& fs);
  ShaderProgram(const std::string& vs, const std::string& fs, const std::string& gs);
  ShaderProgram(GLuint program);

  static ShaderProgram load_from_file(const std::string& vs_file,
                                      const std::string& fs_file,
                                      const std::string& gs_file);

  void use();

  GLuint id();
};

#endif  // SHADER__H
