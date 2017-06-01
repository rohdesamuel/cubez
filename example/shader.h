#ifndef SHADER__H
#define SHADER__H

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

#include "inc/cubez.h"
#include "inc/common.h"

#include <iostream>

class ShaderProgram {
 private:
  GLuint create_shader(const char* shader, GLenum shader_type) {
    GLuint s = glCreateShader(shader_type);
    glShaderSource(s, 1, &shader, nullptr);
    glCompileShader(s);
    GLint success = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      GLint log_size = 0;
      glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_size);
      char* error = new char[log_size];

      glGetShaderInfoLog(s, log_size, &log_size, error);
      std::cout << "Error in shader compilation: " << error << std::endl;
      delete[] error;
    }
    return s;
  }


  GLuint program_;

 public:
  ShaderProgram(const std::string& vs, const std::string& fs) {
    GLuint v = create_shader(vs.c_str(), GL_VERTEX_SHADER);
    GLuint f = create_shader(fs.c_str(), GL_FRAGMENT_SHADER);
    program_ = glCreateProgram();
    glAttachShader(program_, f);
    glAttachShader(program_, v);
    glLinkProgram(program_);
    glDetachShader(program_, f);
    glDetachShader(program_, v);
    glDeleteShader(v);
    glDeleteShader(f);
  }

  ~ShaderProgram() {
    glDeleteProgram(program_);
  }

  void use() {
    glUseProgram(program_);
  }
};

#endif  // SHADER__H
