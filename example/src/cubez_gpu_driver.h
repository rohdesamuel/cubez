#ifndef CUBEZ_GPU_DRIVER__H
#define CUBEZ_GPU_DRIVER__H

#include <Ultralight/platform/GPUDriver.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <vector>
#include <map>

namespace cubez
{

typedef ultralight::ShaderType ProgramType;

// Not thread-safe.
class CubezGpuDriver : public ultralight::GPUDriver {
public:
  CubezGpuDriver(GLuint viewport_width, GLuint viewport_height, GLfloat scale);

  virtual ~CubezGpuDriver() {}

  virtual void BeginSynchronize() override {}

  virtual void EndSynchronize() override {}

  virtual uint32_t NextTextureId() override {
    return next_texture_id_++;
  }

  virtual void CreateTexture(uint32_t texture_id,
                             ultralight::Ref<ultralight::Bitmap> bitmap) override;

  virtual void UpdateTexture(uint32_t texture_id,
                             ultralight::Ref<ultralight::Bitmap> bitmap) override;

  virtual void BindTexture(uint8_t texture_unit,
                           uint32_t texture_id) override;

  virtual void DestroyTexture(uint32_t texture_id) override;

  virtual uint32_t NextRenderBufferId() override {
    return next_render_buffer_id_++;
  }

  virtual void CreateRenderBuffer(uint32_t render_buffer_id,
                                  const ultralight::RenderBuffer& buffer) override;

  virtual void BindRenderBuffer(uint32_t render_buffer_id) override;

  virtual void SetRenderBufferViewport(uint32_t render_buffer_id,
                                       uint32_t width,
                                       uint32_t height) override;

  virtual void ClearRenderBuffer(uint32_t render_buffer_id) override;

  virtual void DestroyRenderBuffer(uint32_t render_buffer_id) override;

  virtual uint32_t NextGeometryId() override {
    return next_geometry_id_++;
  }

  virtual void CreateGeometry(uint32_t geometry_id,
                              const ultralight::VertexBuffer& vertices,
                              const ultralight::IndexBuffer& indices) override;

  virtual void UpdateGeometry(uint32_t geometry_id,
                              const ultralight::VertexBuffer& vertices,
                              const ultralight::IndexBuffer& buffer) override;

  virtual void DrawGeometry(uint32_t geometry_id,
                            uint32_t indices_count,
                            uint32_t indices_offset,
                            const ultralight::GPUState& state) override;

  virtual void DestroyGeometry(uint32_t geometry_id) override;

  virtual void UpdateCommandList(const ultralight::CommandList& list) override;

  virtual bool HasCommandsPending() override {
    return !command_list.empty();
  }

  virtual void DrawCommandList() override;

  int batch_count() {
    return batch_count_;
  }

  void ResizeViewport(int width, int height);

  void BindUltralightTexture(uint32_t ultralight_texture_id);

  void LoadPrograms();
  void DestroyPrograms();

  void LoadProgram(ProgramType type, const char* vert, const char* frag);
  void SelectProgram(ProgramType type);
  void SetUniformDefaults();
  void SetUniform1ui(const char* name, GLuint val);
  void SetUniform1f(const char* name, float val);
  void SetUniform1fv(const char* name, size_t count, const float* val);
  void SetUniform4f(const char* name, const float val[4]);
  void SetUniform4fv(const char* name, size_t count, const float* val);
  void SetUniformMatrix4fv(const char* name, size_t count, const float* val);

protected:
  std::map<int, GLuint> texture_map;
  uint32_t next_texture_id_ = 1;
  uint32_t next_render_buffer_id_ = 1; // 0 is reserved for default render buffer
  uint32_t next_geometry_id_ = 1;

  struct GeometryEntry {
    GLuint vao; // VAO id
    GLuint vbo_vertices; // VBO id for vertices
    GLuint vbo_indices; // VBO id for indices
  };
  std::map<int, GeometryEntry> geometry_map;

  struct FrameBufferInfo {
    GLuint id;
    GLuint width;
    GLuint height;
  };
  std::map<int, FrameBufferInfo> frame_buffer_map;

  struct ProgramEntry {
    GLuint program_id;
    GLuint vert_shader_id;
    GLuint frag_shader_id;
  };
  std::map<ProgramType, ProgramEntry> programs_;
  GLuint cur_program_id_;

  std::vector<ultralight::Command> command_list;
  int batch_count_;
  GLuint viewport_width_, viewport_height_;
  GLfloat render_buffer_width_, render_buffer_height_;
  GLfloat scale_;
};

}  // namespace ultralight


#endif  // CUBEZ_GPU_DRIVER__H