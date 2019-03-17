#include "opengl_render_module.h"
#include "render.h"
#include "cubez_gpu_driver.h"

#include <iostream>

void LockState(qbRenderImpl impl) {
  impl->driver->BeginSynchronize();
}

void UnlockState(qbRenderImpl impl) {
  impl->driver->EndSynchronize();
}

uint32_t NextTextureId(qbRenderImpl impl) {
  return impl->driver->NextTextureId();
}

void CreateTexture(qbRenderImpl impl,
                   uint32_t texture_id,
                   qbPixelMap pixel_map) {
  ultralight::BitmapFormat format =
    qb_pixelmap_format(pixel_map) == QB_PIXEL_FORMAT_RGBA8 ?
    ultralight::BitmapFormat::kBitmapFormat_RGBA8 :
    ultralight::BitmapFormat::kBitmapFormat_A8;

  auto bm = ultralight::Bitmap::Create(qb_pixelmap_width(pixel_map),
                             qb_pixelmap_height(pixel_map),
                             format,
                             qb_pixelmap_rowsize(pixel_map),
                             qb_pixelmap_pixels(pixel_map),
                             qb_pixelmap_size(pixel_map));
  impl->driver->CreateTexture(texture_id, bm);
}

void UpdateTexture(qbRenderImpl impl,
                   uint32_t texture_id,
                   qbPixelMap pixel_map) {
  ultralight::BitmapFormat format =
    qb_pixelmap_format(pixel_map) == QB_PIXEL_FORMAT_RGBA8 ?
    ultralight::BitmapFormat::kBitmapFormat_RGBA8 :
    ultralight::BitmapFormat::kBitmapFormat_A8;

  auto bm = ultralight::Bitmap::Create(qb_pixelmap_width(pixel_map),
                                       qb_pixelmap_height(pixel_map),
                                       format,
                                       qb_pixelmap_rowsize(pixel_map),
                                       qb_pixelmap_pixels(pixel_map),
                                       qb_pixelmap_size(pixel_map));

  impl->driver->UpdateTexture(texture_id, bm);
}

void qb_rendermodule_usetexture(qbRenderImpl impl,
                                uint32_t texture_id,
                                uint32_t texture_unit,
                                uint32_t target) {
  impl->driver->BindTexture(texture_id, texture_unit, target);
}

void DestroyTexture(qbRenderImpl impl,
                    uint32_t texture_id) {
  impl->driver->DestroyTexture(texture_id);
}

uint32_t NextRenderBufferId(qbRenderImpl impl) {
  return impl->driver->NextRenderBufferId();
}

void CreateRenderBuffer(qbRenderImpl impl,
                        uint32_t render_buffer_id,
                        const qbFrameBuffer render_buffer,
                        const qbFrameBufferAttr attr) {
}

void BindRenderBuffer(qbRenderImpl impl,
                      uint32_t render_buffer_id) {
  impl->driver->BindRenderBuffer(render_buffer_id);
}

void SetRenderBufferViewport(qbRenderImpl impl,
                             uint32_t render_buffer_id,
                             uint32_t width,
                             uint32_t height) {
  impl->driver->SetRenderBufferViewport(render_buffer_id, width, height);
}

void ClearRenderBuffer(qbRenderImpl impl,
                       uint32_t render_buffer_id) {
  impl->driver->ClearRenderBuffer(render_buffer_id);
}

void DestroyRenderBuffer(qbRenderImpl impl,
                         uint32_t render_buffer_id) {
  impl->driver->DestroyRenderBuffer(render_buffer_id);
}

uint32_t NextGeometryId(qbRenderImpl impl) {
  
  return impl->driver->NextGeometryId();
}

void CreateGeometry(qbRenderImpl impl,
                    uint32_t geometry_id,
                    const qbVertexBuffer vertices,
                    const qbIndexBuffer indices,
                    const qbGpuBuffer buffers, size_t buffer_size,
                    const qbVertexAttribute attributes, size_t attributes_size) {
  ultralight::VertexBuffer vb;
  vb.data = nullptr; // (uint8_t*)vertices->data;
  vb.size = 0; // vertices->size;

  ultralight::IndexBuffer ib;
  ib.data = nullptr; // (uint8_t*)indices->data;
  ib.size = 0; // indices->size;

  impl->driver->CreateGeometry(geometry_id, vb, ib, buffers,
                               buffer_size, attributes, attributes_size);
}

void UpdateGeometry(qbRenderImpl impl,
                    uint32_t geometry_id,
                    const qbVertexBuffer vertices,
                    const qbIndexBuffer indices,
                    const qbGpuBuffer buffers, size_t buffer_size) {
  ultralight::VertexBuffer vb;
  vb.data = nullptr; // (uint8_t*)vertices->data;
  vb.size = 0; // vertices->size;

  ultralight::IndexBuffer ib;
  ib.data = nullptr; // (uint8_t*)indices->data;
  ib.size = 0; // indices->size;

  impl->driver->UpdateGeometry(geometry_id, vb, ib, buffers, buffer_size);
}

void DestroyGeometry(qbRenderImpl impl,
                     uint32_t geometry_id) {
  impl->driver->DestroyGeometry(geometry_id);
}

void DrawGeometry(qbRenderImpl impl,
                  uint32_t geometry_id,
                  size_t indices_count,
                  size_t indices_offest) {
  impl->driver->DrawGeometry(geometry_id, indices_count, indices_offest);
}

void Render(qbRenderImpl impl, qbRenderEvent event) {
  impl->driver->BindRenderBuffer(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Draw
}

void OnDestroy(qbRenderImpl impl) {
}

uint32_t LoadProgram(qbRenderImpl impl, const char* vs_file, const char* fs_file) {
  return impl->driver->LoadProgram(vs_file, fs_file);
}

void UseProgram(qbRenderImpl impl, uint32_t program) {
  impl->driver->UseProgram(program);
}

qbRenderModule Initialize(uint32_t viewport_width, uint32_t viewport_height, float scale) {
  qb_render_makecurrent();

  cubez::CubezGpuDriver* driver = new cubez::CubezGpuDriver(
    viewport_width, viewport_height, scale);

  qbRenderImpl impl = new qbRenderImpl_{ driver };

  qbRenderInterface interface = nullptr;
  //interface.createtexture = CreateTexture;
  //interface.createframebuffer = CreateRenderBuffer;

  qbRenderModule module = nullptr;
  qb_rendermodule_create(&module, interface, impl, viewport_width, viewport_height, scale);

  return module;
}