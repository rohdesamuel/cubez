#include "cubez_gpu_driver.h"
#include <cubez/common.h>

#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/FileSystem.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "opengl_render_module.h"
#include "shader.h"

#define SHADER_PATH "shaders/"

static void ReadFile(const char* filepath, std::string& result) {
  // To maintain predictable behavior across platforms we use
  // whatever FileSystem that Ultralight is using:
  ultralight::FileSystem* fs = ultralight::Platform::instance().file_system();
  if (!fs) {
    FATAL("No FileSystem defined.");
  }

  auto handle = fs->OpenFile(filepath, false);
  if (handle == ultralight::invalidFileHandle) {
    FATAL("Could not open file path: " << filepath);
  }

  int64_t fileSize = 0;
  fs->GetFileSize(handle, fileSize);
  result.resize((size_t)fileSize);
  fs->ReadFromFile(handle, &result[0], fileSize);
  fs->CloseFile(handle);
}

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

static GLuint LoadShaderFromFile(GLenum shader_type, const char* filename) {
  std::string shader_source;
  std::string path = std::string(SHADER_PATH) + filename;
  ReadFile(path.c_str(), shader_source);
  GLint compileStatus;
  const char* shader_source_str = shader_source.c_str();
  GLuint shader_id = glCreateShader(shader_type);
  glShaderSource(shader_id, 1, &shader_source_str, NULL);
  glCompileShader(shader_id);
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compileStatus);
  if (compileStatus == GL_FALSE)
    FATAL("Unable to compile shader. Filename: " << filename << "\n\tError:"
          << glErrorString(glGetError()) << "\n\tLog: " << GetShaderLog(shader_id))
  return shader_id;
}

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

namespace cubez
{

CubezGpuDriver::CubezGpuDriver(GLuint viewport_width, GLuint viewport_height, GLfloat scale) :
  viewport_width_(viewport_width),
  viewport_height_(viewport_height),
  scale_(scale) {
  // Render Buffer ID 0 is reserved for the screen, set its viewport dimensions now.
  frame_buffer_map[0] = { 0, viewport_width, viewport_height };
}

void CubezGpuDriver::CreateTexture(uint32_t texture_id,
                                   ultralight::Ref<ultralight::Bitmap> bitmap) {
  GLuint tex_id;
  glGenTextures(1, &tex_id);
  texture_map[texture_id] = tex_id;

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, tex_id);

  CHECK_GL();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  CHECK_GL();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CHECK_GL();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, bitmap->row_bytes() / bitmap->bpp());
  CHECK_GL();

  if (bitmap->format() == ultralight::kBitmapFormat_A8) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, bitmap->width(), bitmap->height(), 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap->pixels());
  } else if (bitmap->format() == ultralight::kBitmapFormat_RGBA8) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, bitmap->pixels());
  } else {
    FATAL("Unhandled texture format: " << bitmap->format())
  }

  CHECK_GL();
  glGenerateMipmap(GL_TEXTURE_2D);
  CHECK_GL();
}

void CubezGpuDriver::UpdateTexture(uint32_t texture_id,
                                   ultralight::Ref<ultralight::Bitmap> bitmap) {
  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_2D, texture_map[texture_id]);
  CHECK_GL();
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, bitmap->row_bytes() / bitmap->bpp());

  if (!bitmap->IsEmpty()) {
    if (bitmap->format() == ultralight::kBitmapFormat_A8)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, bitmap->width(), bitmap->height(), 0,
                   GL_RED, GL_UNSIGNED_BYTE, bitmap->pixels());
    else if (bitmap->format() == ultralight::kBitmapFormat_RGBA8)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width(), bitmap->height(), 0,
                   GL_BGRA, GL_UNSIGNED_BYTE, bitmap->pixels());
    else
      FATAL("Unhandled texture format: " << bitmap->format());

    CHECK_GL();
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  CHECK_GL();
}

void CubezGpuDriver::BindTexture(uint8_t texture_unit, uint32_t texture_id) {
  glActiveTexture(GL_TEXTURE0 + texture_unit);
  glBindTexture(GL_TEXTURE_2D, texture_map[texture_id]);
  CHECK_GL();
}

void CubezGpuDriver::BindTexture(uint32_t texture_id,
                                 uint32_t texture_unit,
                                 uint32_t target) {
  glActiveTexture(texture_unit);
  glBindTexture(target, texture_id);
  CHECK_GL();
}

void CubezGpuDriver::DestroyTexture(uint32_t texture_id) {
  GLuint tex_id = texture_map[texture_id];
  glDeleteTextures(1, &tex_id);
}

void CubezGpuDriver::CreateRenderBuffer(uint32_t render_buffer_id,
                                        const ultralight::RenderBuffer& buffer) {
  if (render_buffer_id == 0) {
    INFO("Should not be reached! Render Buffer ID 0 is reserved for default framebuffer.");
    return;
  }

  GLuint frame_buffer_id = 0;
  glGenFramebuffers(1, &frame_buffer_id);
  CHECK_GL();
  frame_buffer_map[render_buffer_id] = { frame_buffer_id,
    (GLuint)buffer.viewport_width, (GLuint)buffer.viewport_height };
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
  CHECK_GL();

  glViewport(0, 0, buffer.viewport_width, buffer.viewport_height);

  GLuint rbuf_texture_id = texture_map[buffer.texture_id];
  glBindTexture(GL_TEXTURE_2D, rbuf_texture_id);
  CHECK_GL();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rbuf_texture_id, 0);
  CHECK_GL();

  GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(1, drawBuffers);
  CHECK_GL();

  GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (result != GL_FRAMEBUFFER_COMPLETE) {
    FATAL("Error creating FBO: " << result);
  }
  CHECK_GL();
}

void CubezGpuDriver::BindRenderBuffer(uint32_t render_buffer_id) {
  FrameBufferInfo framebuffer = frame_buffer_map[render_buffer_id];
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.id);
  CHECK_GL();
  render_buffer_width_ = (GLfloat)framebuffer.width;
  render_buffer_height_ = (GLfloat)framebuffer.height;
  glViewport(0, 0, static_cast<GLsizei>(framebuffer.width * scale_),
                   static_cast<GLsizei>(framebuffer.height * scale_));
}

void CubezGpuDriver::SetRenderBufferViewport(uint32_t render_buffer_id, uint32_t width, uint32_t height) {
  auto i = frame_buffer_map.find(render_buffer_id);
  if (i != frame_buffer_map.end()) {
    i->second.width = width;
    i->second.height = height;
  }
}

void CubezGpuDriver::ClearRenderBuffer(uint32_t render_buffer_id) {
  BindRenderBuffer(render_buffer_id);
  glDisable(GL_SCISSOR_TEST);
  CHECK_GL();
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  CHECK_GL();
  glClear(GL_COLOR_BUFFER_BIT);
  CHECK_GL();
}

void CubezGpuDriver::DestroyRenderBuffer(uint32_t render_buffer_id) {
  if (render_buffer_id == 0)
    return;
  FrameBufferInfo framebuffer = frame_buffer_map[render_buffer_id];
  glDeleteFramebuffers(1, &framebuffer.id);
}


void CubezGpuDriver::CreateGeometry(uint32_t geometry_id,
                                    const ultralight::VertexBuffer& vertices,
                                    const ultralight::IndexBuffer& indices) {
  /*static GLsizei stride = 140;
  static qbVertexAttribute_ attributes[] = /*{
    { 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0 },
    { 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)8 },
    { 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)12 },
    { 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)20 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)28 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)44 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)60 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)76 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)92 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)108 },
    { 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)124 },
  };

  CreateGeometry(geometry_id, vertices, indices,
                 nullptr, 0,
                 attributes, sizeof(attributes) / sizeof(qbVertexAttribute_));*/
}

void CubezGpuDriver::CreateGeometry(uint32_t geometry_id,
                                    const ultralight::VertexBuffer& vertices,
                                    const ultralight::IndexBuffer& indices,
                                    const qbGpuBuffer buffers, size_t buffer_size,
                                    const qbVertexAttribute attributes, size_t attributes_size) {
  GeometryEntry geometry;
  glGenVertexArrays(1, &geometry.vao);
  glBindVertexArray(geometry.vao);

  glGenBuffers(1, &geometry.vbo_vertices);
  glBindBuffer(GL_ARRAY_BUFFER, geometry.vbo_vertices);
  glBufferData(GL_ARRAY_BUFFER, vertices.size, vertices.data, GL_DYNAMIC_DRAW);

  if (attributes) {
    for (size_t attribute_pos = 0; attribute_pos < attributes_size; ++attribute_pos) {
      qbVertexAttribute attr = attributes + attribute_pos;
      //glVertexAttribPointer(attribute_pos, attr->size, attr->type,
        //attr->normalized, attr->stride, attr->offset);
    }
    CHECK_GL();

    for (size_t attribute_pos = 0; attribute_pos < attributes_size; ++attribute_pos) {
      glEnableVertexAttribArray(attribute_pos++);
    }
    CHECK_GL();
  }

  glGenBuffers(1, &geometry.vbo_indices);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry.vbo_indices);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size, indices.data,
               GL_STATIC_DRAW);
  CHECK_GL();

  /*if (buffers) {
    for (size_t i = 0; i < buffer_size; ++i) {
      qbGpuBuffer buffer = buffers + i;
      GLuint id;
      glGenBuffers(1, &id);
      glBindBuffer(buffer->buffer_type, id);
      glBufferData(buffer->buffer_type, buffer->size, buffer->data, buffer->sharing_type);
      buffer->id = id;
      CHECK_GL();
    }
  }*/

  geometry_map[geometry_id] = geometry;

  glBindVertexArray(0);
  CHECK_GL();
}

void CubezGpuDriver::UpdateGeometry(uint32_t geometry_id,
                                    const ultralight::VertexBuffer& vertices,
                                    const ultralight::IndexBuffer& indices) {
  UpdateGeometry(geometry_id, vertices, indices, nullptr, 0);
}

void CubezGpuDriver::UpdateGeometry(uint32_t geometry_id,
                                    const ultralight::VertexBuffer& vertices,
                                    const ultralight::IndexBuffer& indices,
                                    const qbGpuBuffer buffers, size_t buffer_size) {
  GeometryEntry& geometry = geometry_map[geometry_id];
  glBindVertexArray(geometry.vao);
  CHECK_GL();
  glBindBuffer(GL_ARRAY_BUFFER, geometry.vbo_vertices);
  glBufferData(GL_ARRAY_BUFFER, vertices.size, vertices.data, GL_DYNAMIC_DRAW);
  CHECK_GL();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry.vbo_indices);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size, indices.data, GL_STATIC_DRAW);
  CHECK_GL();
  glBindVertexArray(0);
  CHECK_GL();

  if (buffers) {
    /*for (size_t i = 0; i < buffer_size; ++i) {
      qbGpuBuffer buffer = buffers + i;
      glBindBuffer(buffer->buffer_type, buffer->id);
      glBufferData(buffer->buffer_type, buffer->size, buffer->data, buffer->sharing_type);
      CHECK_GL();
    }*/
  }
}

void CubezGpuDriver::DrawGeometry(uint32_t geometry_id,
                                  uint32_t indices_count,
                                  uint32_t indices_offset,
                                  const ultralight::GPUState& state) {
  if (programs_.empty())
    LoadPrograms();

  BindRenderBuffer(state.render_buffer_id);

  GeometryEntry& geometry = geometry_map[geometry_id];
  SelectProgram((ProgramType)state.shader_type);
  SetUniformDefaults();
  CHECK_GL();
  if (state.render_buffer_id == 0) {
    // Anything drawn to screen needs to be flipped vertically to compensate for
    // flipping in vertex shader.
    ultralight::Matrix4x4 mat = state.transform;
    mat.data[4] *= -1.0;
    mat.data[5] *= -1.0;
    mat.data[12] += -render_buffer_height_ * mat.data[4];
    mat.data[13] += -render_buffer_height_ * mat.data[5];
    SetUniformMatrix4fv("Transform", 1, &mat.data[0]);
  } else {
    SetUniformMatrix4fv("Transform", 1, &state.transform.data[0]);
  }
  CHECK_GL();
  SetUniform4fv("Scalar4", 2, &state.uniform_scalar[0]);
  CHECK_GL();
  SetUniform4fv("Vector", 8, &state.uniform_vector[0].value[0]);
  CHECK_GL();
  SetUniform1ui("ClipSize", state.clip_size);
  CHECK_GL();
  SetUniformMatrix4fv("Clip", 8, &state.clip[0].data[0]);

  CHECK_GL();

  glBindVertexArray(geometry.vao);

  CHECK_GL();

  BindTexture(0, state.texture_1_id);
  BindTexture(1, state.texture_2_id);
  BindTexture(2, state.texture_3_id);

  CHECK_GL();

  glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT,
    (GLvoid*)(indices_offset * sizeof(unsigned int)));

  glBindVertexArray(0);

  batch_count_++;

  CHECK_GL();
}
void CubezGpuDriver::DrawGeometry(uint32_t geometry_id,
                                  uint32_t indices_count,
                                  uint32_t indices_offset) {
  GeometryEntry& geometry = geometry_map[geometry_id];

  glBindVertexArray(geometry.vao);

  CHECK_GL();

  glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT,
    (GLvoid*)(indices_offset * sizeof(unsigned int)));

  glBindVertexArray(0);

  CHECK_GL();
}

void CubezGpuDriver::DestroyGeometry(uint32_t geometry_id) {
  GeometryEntry& geometry = geometry_map[geometry_id];

  glDeleteBuffers(1, &geometry.vbo_indices);
  glDeleteBuffers(1, &geometry.vbo_vertices);

  glDeleteVertexArrays(1, &geometry.vao);

  geometry_map.erase(geometry_id);
}

void CubezGpuDriver::UpdateCommandList(const ultralight::CommandList& list) {
  if (list.size) {
    command_list.resize(list.size);
    memcpy(&command_list[0], list.commands, sizeof(ultralight::Command) * list.size);
  }
}

void CubezGpuDriver::DrawCommandList() {
  if (command_list.empty())
    return;

  CHECK_GL();

  batch_count_ = 0;

  glEnable(GL_BLEND);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_NEVER);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  CHECK_GL();

  for (auto i = command_list.begin(); i != command_list.end(); ++i) {
    switch (i->command_type) {
      case ultralight::kCommandType_DrawGeometry:
        DrawGeometry(i->geometry_id, i->indices_count, i->indices_offset, i->gpu_state);
        break;
      case ultralight::kCommandType_ClearRenderBuffer:
        ClearRenderBuffer(i->gpu_state.render_buffer_id);
        break;
    };
  }

  command_list.clear();
  glDisable(GL_SCISSOR_TEST);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CubezGpuDriver::BindUltralightTexture(uint32_t ultralight_texture_id) {
  GLuint tex_id = texture_map[ultralight_texture_id];
  glBindTexture(GL_TEXTURE_2D, tex_id);
}

void CubezGpuDriver::ResizeViewport(int width, int height) {
  SetRenderBufferViewport(0, width, height);
}

void CubezGpuDriver::LoadPrograms(void) {
  const char* vert_program = "vs/v2f_c4f_t2f_t2f_d28f.vert";
  LoadProgram(ultralight::kShaderType_Fill, vert_program, "ps/fill.frag");
}

void CubezGpuDriver::DestroyPrograms(void) {
  glUseProgram(0);
  for (auto i = programs_.begin(); i != programs_.end(); i++) {
    ProgramEntry& prog = i->second;
    glDetachShader(prog.program_id, prog.vert_shader_id);
    glDetachShader(prog.program_id, prog.frag_shader_id);
    glDeleteShader(prog.vert_shader_id);
    glDeleteShader(prog.frag_shader_id);
    glDeleteProgram(prog.program_id);
  }
  programs_.clear();
}

void CubezGpuDriver::LoadProgram(ProgramType type,
                              const char* vert, const char* frag) {
  ProgramEntry prog;
  prog.vert_shader_id = LoadShaderFromFile(GL_VERTEX_SHADER, vert);
  prog.frag_shader_id = LoadShaderFromFile(GL_FRAGMENT_SHADER, frag);
  prog.program_id = glCreateProgram();
  glAttachShader(prog.program_id, prog.vert_shader_id);
  glAttachShader(prog.program_id, prog.frag_shader_id);

  glBindAttribLocation(prog.program_id, 0, "in_Position");
  glBindAttribLocation(prog.program_id, 1, "in_Color");
  glBindAttribLocation(prog.program_id, 2, "in_TexCoord");
  glBindAttribLocation(prog.program_id, 3, "in_ObjCoord");
  glBindAttribLocation(prog.program_id, 4, "in_Data0");
  glBindAttribLocation(prog.program_id, 5, "in_Data1");
  glBindAttribLocation(prog.program_id, 6, "in_Data2");
  glBindAttribLocation(prog.program_id, 7, "in_Data3");
  glBindAttribLocation(prog.program_id, 8, "in_Data4");
  glBindAttribLocation(prog.program_id, 9, "in_Data5");
  glBindAttribLocation(prog.program_id, 10, "in_Data6");

  glLinkProgram(prog.program_id);
  glUseProgram(prog.program_id);
  glUniform1i(glGetUniformLocation(prog.program_id, "Texture1"), 0);
  glUniform1i(glGetUniformLocation(prog.program_id, "Texture2"), 1);
  glUniform1i(glGetUniformLocation(prog.program_id, "Texture3"), 2);

  if (glGetError()) {
    FATAL("Unable to link shader.\n\tError:" << glErrorString(glGetError()) << "\n\tLog: " << GetProgramLog(prog.program_id));
  }

  glDetachShader(prog.program_id, prog.frag_shader_id);
  glDetachShader(prog.program_id, prog.vert_shader_id);
  glDeleteShader(prog.vert_shader_id);
  glDeleteShader(prog.frag_shader_id);

  programs_[type] = prog;
}

uint32_t CubezGpuDriver::LoadProgram(const char* vert, const char* frag) {
  return ShaderProgram::load_from_file(vert, frag).id();
}

void CubezGpuDriver::SelectProgram(ProgramType type) {
  auto i = programs_.find(type);
  if (i != programs_.end()) {
    cur_program_id_ = i->second.program_id;
    glUseProgram(i->second.program_id);
  } else {
    FATAL("Missing shader type: " << type);
  }
}

void CubezGpuDriver::SetUniformDefaults() {
  float params[4] = { 0.0f, (float)render_buffer_width_, (float)render_buffer_height_, scale_ };
  SetUniform4f("State", params);
}

void CubezGpuDriver::SetUniform1ui(const char* name, GLuint val) {
  glUniform1ui(glGetUniformLocation(cur_program_id_, name), val);
}

void CubezGpuDriver::SetUniform1f(const char* name, float val) {
  glUniform1f(glGetUniformLocation(cur_program_id_, name), (GLfloat)val);
}

void CubezGpuDriver::SetUniform1fv(const char* name, size_t count, const float* val) {
  glUniform1fv(glGetUniformLocation(cur_program_id_, name), (GLsizei)count, val);
}

void CubezGpuDriver::SetUniform4f(const char* name, const float val[4]) {
  glUniform4f(glGetUniformLocation(cur_program_id_, name),
    (GLfloat)val[0], (GLfloat)val[1], (GLfloat)val[2], (GLfloat)val[3]);
}

void CubezGpuDriver::SetUniform4fv(const char* name, size_t count, const float* val) {
  glUniform4fv(glGetUniformLocation(cur_program_id_, name), (GLsizei)count, val);
}

void CubezGpuDriver::SetUniformMatrix4fv(const char* name, size_t count, const float* val) {
  glUniformMatrix4fv(glGetUniformLocation(cur_program_id_, name), (GLsizei)count, false, val);
}

void CubezGpuDriver::UseProgram(uint32_t program) {

}

}  // namespace ultralight
