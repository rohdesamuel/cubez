#include "shader.h"
#include <filesystem>

#ifdef __COMPILE_AS_WINDOWS__
namespace std
{
namespace filesystem = std::experimental::filesystem;
}
#endif  // __COMPILE_AS_WINDOWS__

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

GLuint ShaderProgram::create_shader(const char* shader, GLenum shader_type) {
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
    FATAL("Unable to compile shader \n" << shader << ".\n\tError:" << glErrorString(glGetError()) << "\n\tLog: " << error);
    delete[] error;
  }
  return s;
}

ShaderProgram::ShaderProgram(const std::string& vs, const std::string& fs) {
  program_ = glCreateProgram();
  GLuint v = create_shader(vs.c_str(), GL_VERTEX_SHADER);
  GLuint f = create_shader(fs.c_str(), GL_FRAGMENT_SHADER);
  
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
    printf("Shader program information for program %d\n", program_);
    GLint count;

    GLint size; // size of the variable
    GLenum type; // type of the variable (float, vec3 or mat4, etc)

    const GLsizei bufSize = 16; // maximum name length
    GLchar name[bufSize]; // variable name in GLSL
    GLsizei length; // name length
    
    // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
    // https://www.khronos.org/opengl/wiki/Program_Introspection
    glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);
    printf("Active Uniforms: %d\n", count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniform(program_, (GLuint)i, bufSize, &length, &size, &type, name);
      printf("Uniform #%d Type: %u Name: %s\n", i, type, name);

      uint32_t index;
      char *uniformNames[] = { name };
      glGetUniformIndices(program_, 1, uniformNames, &index);
      printf("Index is %d\n", index);

      uint32_t indices[] = { i };
      int32_t block_index;
      glGetActiveUniformsiv(program_, 1, indices, GL_UNIFORM_BLOCK_INDEX, &block_index);
      printf("Block index is %d\n", block_index);
    }

    glGetProgramiv(program_, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    printf("Active UniformsBlocks: %d\n", count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniformBlockName(program_, (GLuint)i, bufSize, &length, name);
      printf("UniformBlock #%d Type: %u Name: %s\n", i, type, name);
    }
    printf("\n");
  }

  glDetachShader(program_, f);
  glDetachShader(program_, v);
  glDeleteShader(v);
  glDeleteShader(f);
}

ShaderProgram::ShaderProgram(const std::string& vs, const std::string& fs, const std::string& gs) {
  program_ = glCreateProgram();
  GLuint v = create_shader(vs.c_str(), GL_VERTEX_SHADER);
  GLuint f = create_shader(fs.c_str(), GL_FRAGMENT_SHADER);
  GLuint g = create_shader(gs.c_str(), GL_GEOMETRY_SHADER);

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
    printf("Shader program information for program %d\n", program_);
    GLint count;

    GLint size; // size of the variable
    GLenum type; // type of the variable (float, vec3 or mat4, etc)

    const GLsizei bufSize = 16; // maximum name length
    GLchar name[bufSize]; // variable name in GLSL
    GLsizei length; // name length

                    // https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout
                    // https://www.khronos.org/opengl/wiki/Program_Introspection
    glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);
    printf("Active Uniforms: %d\n", count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniform(program_, (GLuint)i, bufSize, &length, &size, &type, name);
      printf("Uniform #%d Type: %u Name: %s\n", i, type, name);

      uint32_t index;
      char *uniformNames[] = { name };
      glGetUniformIndices(program_, 1, uniformNames, &index);
      printf("Index is %d\n", index);

      uint32_t indices[] = { i };
      int32_t block_index;
      glGetActiveUniformsiv(program_, 1, indices, GL_UNIFORM_BLOCK_INDEX, &block_index);
      printf("Block index is %d\n", block_index);
    }

    glGetProgramiv(program_, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    printf("Active UniformsBlocks: %d\n", count);

    for (int32_t i = 0; i < count; i++) {
      glGetActiveUniformBlockName(program_, (GLuint)i, bufSize, &length, name);
      printf("UniformBlock #%d Type: %u Name: %s\n", i, type, name);
    }
    printf("\n");
  }

  glDetachShader(program_, f);
  glDetachShader(program_, v);
  glDetachShader(program_, g);
  glDeleteShader(v);
  glDeleteShader(f);
  glDeleteShader(g);
}


ShaderProgram::ShaderProgram(GLuint program) : program_(program) {}

ShaderProgram ShaderProgram::load_from_file(const std::string& vs_file,
                                            const std::string& fs_file,
                                            const std::string& gs_file) {
  
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
    return ShaderProgram(vs, fs);
  }
  return ShaderProgram(vs, fs, gs);
}

void ShaderProgram::use() {
  glUseProgram(program_);
}

GLuint ShaderProgram::id() {
  return program_;
}

