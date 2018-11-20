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

    GLint is_linked;
    glGetProgramiv(program_, GL_LINK_STATUS, &is_linked);
    if(is_linked == GL_FALSE) {
      // Noticed that glGetProgramiv is used to get the length for a shader
      // program, not glGetShaderiv.
      int log_size = 0;
      glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_size);

      // The maxLength includes the NULL character.
      char* error = new char[log_size];

      // Notice that glGetProgramInfoLog, not glGetShaderInfoLog.
      glGetProgramInfoLog(program_, log_size, &log_size, error);
      std::cout << "Error in program linkage: " << error << std::endl;

      delete[] error;
      return;
    }

    glDetachShader(program_, f);
    glDetachShader(program_, v);
    glDeleteShader(v);
    glDeleteShader(f);
  }

  ShaderProgram(GLuint program) : program_(program) {}

  static ShaderProgram load_from_file(const std::string& vs_file,
                                      const std::string& fs_file) {
    std::string vs;
    std::string fs;
    {
      std::ifstream t(vs_file);

      t.seekg(0, std::ios::end);   
      vs.reserve(t.tellg());
      t.seekg(0, std::ios::beg);

      vs.assign(std::istreambuf_iterator<char>(t),
          std::istreambuf_iterator<char>());
    }
    {
      std::ifstream t(fs_file);

      t.seekg(0, std::ios::end);   
      fs.reserve(t.tellg());
      t.seekg(0, std::ios::beg);

      fs.assign(std::istreambuf_iterator<char>(t),
          std::istreambuf_iterator<char>());
    }
    return ShaderProgram(vs, fs);
  }

  void use() {
    glUseProgram(program_);
  }

  GLuint id() {
    return program_;
  }
};

#endif  // SHADER__H
