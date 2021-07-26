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

#include "shader.h"
#include <filesystem>

inline char const* glErrorString(GLenum const err) noexcept {
  switch (err) {
    // OpenGL 2.0+ Errors:
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
      // OpenGL 3.0+ Errors
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default: return "UNKNOWN ERROR";
  }
}

inline std::string GetShaderLog(GLuint shader_id) {
  GLint length, result;
  glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
  std::string str(length, ' ');
  glGetShaderInfoLog(shader_id, (GLsizei)str.length(), &result, &str[0]);
  return str;
}

inline std::string GetProgramLog(GLuint program_id) {
  GLint length, result;
  glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length);
  std::string str(length, ' ');
  glGetProgramInfoLog(program_id, (GLsizei)str.length(), &result, &str[0]);
  return str;
}

std::string extensions_to_string(const std::vector<std::string>& extensions) {
  std::string out;
  for (auto& extension : extensions) {
    out += "#extension " + extension + " : enable\n";
  }
  return out;
}

std::string add_extensions(std::string shader, const std::vector<std::string>& extensions) {
  std::string extension_str = extensions_to_string(extensions);

  auto version_loc = shader.find_first_of("#version");
  if (version_loc == std::string::npos) {
    return shader;
  }

  auto end_of_line = shader.find('\n', version_loc);
  if (end_of_line == std::string::npos) {
    return shader;
  }

  return shader.insert(end_of_line + 1, extension_str);
}

bool is_sampler_uniform(int type) {
  return
    type >= GL_SAMPLER_1D && type <= GL_SAMPLER_2D_SHADOW ||
    type >= GL_SAMPLER_1D_ARRAY && type <= GL_SAMPLER_CUBE_SHADOW ||
    type >= GL_INT_SAMPLER_1D && type <= GL_UNSIGNED_INT_SAMPLER_2D_ARRAY ||
    type >= GL_SAMPLER_2D_MULTISAMPLE && type <= GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY ||
    type >= GL_SAMPLER_BUFFER && type <= GL_UNSIGNED_INT_SAMPLER_BUFFER;
}

GLuint ShaderProgram::create_shader(std::string& shader, GLenum shader_type, const std::vector<std::string>& extensions) {
  GLuint s = glCreateShader(shader_type);
  
  shader = add_extensions(shader, extensions);

  const char* shader_src = shader.c_str();
  glShaderSource(s, 1, &shader_src, nullptr);
  glCompileShader(s);  

  GLint success = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &success);
  
  if (success == GL_FALSE) {
    GLint log_size = 0;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_size);
    char* error = new char[log_size];

    glGetShaderInfoLog(s, log_size, &log_size, error);
    FATAL("Unable to compile shader \n" << shader << ".\n\tError:" << glErrorString(glGetError()) << "\n\tLog: " << error);
    delete[] error;
  }
  return s;
}

ShaderProgram::ShaderProgram(const std::string& vs, const std::string& fs, const std::vector<std::string>& extensions) {
  program_ = glCreateProgram();
  std::string vs_mut = vs;
  std::string fs_mut = fs;
  GLuint v = create_shader(vs_mut, GL_VERTEX_SHADER, extensions);
  GLuint f = create_shader(fs_mut, GL_FRAGMENT_SHADER, extensions);
  
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
    FATAL("Unable to link shader.\n\tError:" << glErrorString(glGetError()) << "\n\tLog: " << GetProgramLog(program_));

    delete[] error;
    return;
  }

  {
    GLint count;

    GLint size; // size of the variable
    GLenum type; // type of the variable (float, vec3 or mat4, etc)

    const GLsizei bufSize = 16; // maximum name length
    GLchar name[bufSize]; // variable name in GLSL
    GLsizei length; // name length
    
    // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
    // https://www.khronos.org/opengl/wiki/Program_Introspection
    glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniform(program_, (GLuint)i, bufSize, &length, &size, &type, name);

      uint32_t index;
      char *uniformNames[] = { name };
      glGetUniformIndices(program_, 1, uniformNames, &index);

      uint32_t indices[] = { (GLuint)i };
      int32_t block_index;
      glGetActiveUniformsiv(program_, 1, indices, GL_UNIFORM_BLOCK_INDEX, &block_index);
    }

    glGetProgramiv(program_, GL_ACTIVE_UNIFORM_BLOCKS, &count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniformBlockName(program_, (GLuint)i, bufSize, &length, name);
    }
  }

  glUseProgram(program_);
  register_texture_slots();
  glUseProgram(0);

  glDetachShader(program_, f);
  glDetachShader(program_, v);
  glDeleteShader(v);
  glDeleteShader(f);
}

ShaderProgram::ShaderProgram(const std::string& vs, const std::string& fs, const std::string& gs, const std::vector<std::string>& extensions) {
  program_ = glCreateProgram();
  std::string vs_mut = vs;
  std::string fs_mut = fs;
  std::string gs_mut = gs;
  GLuint v = create_shader(vs_mut, GL_VERTEX_SHADER, extensions);
  GLuint f = create_shader(fs_mut, GL_FRAGMENT_SHADER, extensions);
  GLuint g = create_shader(gs_mut, GL_GEOMETRY_SHADER, extensions);

  glAttachShader(program_, f);
  glAttachShader(program_, v);
  glAttachShader(program_, g);
  glLinkProgram(program_);

  GLint is_linked;
  glGetProgramiv(program_, GL_LINK_STATUS, &is_linked);
  if (is_linked == GL_FALSE) {
    // Noticed that glGetProgramiv is used to get the length for a shader
    // program, not glGetShaderiv.
    int log_size = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_size);

    // The maxLength includes the NULL character.
    char* error = new char[log_size];

    // Notice that glGetProgramInfoLog, not glGetShaderInfoLog.
    glGetProgramInfoLog(program_, log_size, &log_size, error);
    FATAL("Unable to link shader.\n\tError:" << glErrorString(glGetError()) << "\n\tLog: " << GetProgramLog(program_));

    delete[] error;
    return;
  }

  {
    GLint count;

    GLint size; // size of the variable
    GLenum type; // type of the variable (float, vec3 or mat4, etc)

    const GLsizei bufSize = 16; // maximum name length
    GLchar name[bufSize]; // variable name in GLSL
    GLsizei length; // name length

                    // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
                    // https://www.khronos.org/opengl/wiki/Program_Introspection
    glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniform(program_, (GLuint)i, bufSize, &length, &size, &type, name);

      uint32_t index;
      char *uniformNames[] = { name };
      glGetUniformIndices(program_, 1, uniformNames, &index);

      uint32_t indices[] = { (GLuint)i };
      int32_t block_index;
      glGetActiveUniformsiv(program_, 1, indices, GL_UNIFORM_BLOCK_INDEX, &block_index);
    }

    glGetProgramiv(program_, GL_ACTIVE_UNIFORM_BLOCKS, &count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniformBlockName(program_, (GLuint)i, bufSize, &length, name);
    }
  }

  glDetachShader(program_, f);
  glDetachShader(program_, v);
  glDetachShader(program_, g);
  glDeleteShader(v);
  glDeleteShader(f);
  glDeleteShader(g);

  glUseProgram(program_);
  register_texture_slots();
  glUseProgram(0);
}


ShaderProgram::ShaderProgram(GLuint program) : program_(program) {}

ShaderProgram ShaderProgram::load_from_file(const std::string& vs_file,
                                            const std::string& fs_file,
                                            const std::string& gs_file,
                                            const std::vector<std::string>& extensions) {
  
  auto cwd = std::filesystem::current_path();
  
  std::string vs = "";
  std::string fs = "";
  std::string gs = "";
  {
    std::ifstream t(std::filesystem::current_path().append(vs_file).generic_string());

    t.seekg(0, std::ios::end);   
    vs.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    vs.assign(std::istreambuf_iterator<char>(t),
        std::istreambuf_iterator<char>());
  }
  {
    std::ifstream t(std::filesystem::current_path().append(fs_file));

    t.seekg(0, std::ios::end);   
    fs.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    fs.assign(std::istreambuf_iterator<char>(t),
        std::istreambuf_iterator<char>());
  }

  if (!gs_file.empty()) {
    std::ifstream t(std::filesystem::current_path().append(gs_file));

    t.seekg(0, std::ios::end);
    gs.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    gs.assign(std::istreambuf_iterator<char>(t),
              std::istreambuf_iterator<char>());
  }

  if (gs.empty()) {
    return ShaderProgram(vs, fs, extensions);
  }
  return ShaderProgram(vs, fs, gs, extensions);
}

void ShaderProgram::use() {
  glUseProgram(program_);
}

GLuint ShaderProgram::id() {
  return program_;
}

void ShaderProgram::register_texture_slots() {
  int max_length;
  glGetProgramiv(program_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);
  auto name = std::unique_ptr<char[]>(new char[max_length]);

  int uniform_count;
  glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &uniform_count);

  int texture_slot = 0;
  for (int i = 0; i < uniform_count; i++) {
    GLint size;
    GLenum type;
    glGetActiveUniform(program_, i, max_length, NULL, &size, &type, name.get());
    if (is_sampler_uniform(type)) {
      GLint location = glGetUniformLocation(program_, name.get());
      glUniform1i(location, texture_slot);
      sampler_name_to_texture_slot_[std::string(name.get())] = texture_slot;
      ++texture_slot;
    }
  }
}

GLuint ShaderProgram::texture_slot(const std::string& name)
{
  return sampler_name_to_texture_slot_[name];
}

