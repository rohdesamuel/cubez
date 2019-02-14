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
  ShaderProgram(GLuint program);

  static ShaderProgram load_from_file(const std::string& vs_file,
                                      const std::string& fs_file);

  void use();

  GLuint id();
};

#endif  // SHADER__H
