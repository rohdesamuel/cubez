#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <cubez/gui.h>
#include "gui_internal.h"
#include <atomic>
#include <iostream>
#include <set>
#include <list>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "font_registry.h"
#include "font_render.h"
#include <cglm/struct.h>

// std140 layout
struct GuiUniformCamera {
  alignas(16) mat4s projection;
  
  static uint32_t Binding() {
    return 0;
  }
};

enum GuiRenderMode {
  GUI_RENDER_MODE_SOLID,
  GUI_RENDER_MODE_IMAGE,
  GUI_RENDER_MODE_STRING,
};

// std140 layout
struct GuiUniformModel {
  alignas(16) mat4s modelview;
  alignas(16) vec4s color;
  alignas(4)  GuiRenderMode render_mode;

  static uint32_t Binding() {
    return 1;
  }
};

struct qbWindow_ {
  uint32_t id;
  qbWindow parent;

  // X, Y are window coordinates.
  // Z is depth

  // pos = parent->pos + rel_pos
  vec3s pos;

  // Relative position to the parent. If there is no parent, this is relative
  // to the top-left corner, i.e. absolute position.
  vec3s rel_pos;
  vec2s size;

  GuiUniformModel uniform;
  qbGpuBuffer ubo;
  qbMeshBuffer dbo;

  GuiRenderMode render_mode;

  // True if there is a need to update the buffers.
  bool dirty;

  vec4s color;
  vec4s text_color;
  vec2s scale;
  uint32_t font_size;
  qbTextAlign align;
  qbImage background;
  qbWindowCallbacks_ callbacks;
};

struct TextBox {
  qbWindow_ window;

  const char16_t* text;
  vec4s color;
  vec2s scale;
};

qbRenderPass gui_render_pass;
qbRenderGroup gui_render_group;
qbGpuBuffer window_vbo;
qbGpuBuffer window_ebo;

std::atomic_uint window_id;
std::list<qbWindow> windows;
qbWindow focused;

FontRegistry* font_registry;
const char* kDefaultFont = "opensans";

typename decltype(windows) closed_windows;

void qb_window_updateuniform(qbWindow window);

namespace
{

qbWindow FindClosestWindow() {
  if (windows.empty()) {
    return nullptr;
  }
  return windows.front();
}

qbWindow FindFarthestWindow() {
  if (windows.empty()) {
    return nullptr;
  }
  return windows.back();
}

qbWindow FindClosestWindow(int x, int y) {
  if (windows.empty()) {
    return nullptr;
  }
  for (qbWindow window : windows) {
    if (x >= window->pos.x &&
        x < window->pos.x + window->size.x &&
        y >= window->pos.y &&
        y < window->pos.y + window->size.y) {
      return window;
    }
  }
  return nullptr;
}

qbWindow FindClosestWindow(int x, int y, std::function<bool(qbWindow)> handler) {
  if (windows.empty()) {
    return nullptr;
  }
  qbWindow closest = nullptr;
  for (qbWindow window : windows) {
    if (x >= window->pos.x &&
        x < window->pos.x + window->size.x &&
        y >= window->pos.y &&
        y < window->pos.y + window->size.y) {
      closest = window;
      bool handled = handler(closest);
      if (handled) {
        break;
      }
    }
  }
  return closest;
}

qbWindow FindFarthestWindow(int x, int y) {
  if (windows.empty()) {
    return nullptr;
  }
  qbWindow farthest = nullptr;
  for (qbWindow window : windows) {
    if (x >= window->pos.x &&
        x < window->pos.x + window->size.x &&
        y >= window->pos.y &&
        y < window->pos.y + window->size.y) {
      farthest = window;
    }
  }
  return farthest;
}

void InsertWindow(qbWindow window) {
  windows.insert(windows.begin(), window);
}

void EraseWindow(qbWindow window) {
  auto& it = std::find(windows.begin(), windows.end(), window);
  if (it != windows.end()) {
    windows.erase(it);
  }
}

void MoveToFront(qbWindow window) {
  if (windows.size() > 1) {
    auto& it = std::find(windows.begin(), windows.end(), window);
    windows.erase(it);
    windows.push_front(window);
  }
}

void MoveToBack(qbWindow window) {
  if (windows.size() > 1) {
    auto& it = std::find(windows.begin(), windows.end(), window);
    windows.erase(it);
    windows.push_back(window);
  }
}

}

void gui_initialize() {
  font_registry = new FontRegistry();
  font_registry->Load("resources/fonts/OpenSans-Bold.ttf", "opensans");
}

void gui_shutdown() {
}

void gui_handle_input(qbInputEvent input_event) {
  int x, y;
  qb_get_mouse_position(&x, &y);

  FindClosestWindow(x, y, [input_event](qbWindow closest){
    bool handled = false;
    if (input_event->type == QB_INPUT_EVENT_KEY) {
      return true;
    }

    qbMouseEvent e = &input_event->mouse_event;
    if (e->type == QB_MOUSE_EVENT_BUTTON) {
      if (e->button.button == QB_BUTTON_LEFT && e->button.state == QB_MOUSE_DOWN) {
        if (closest) {
          //qb_window_movetofront(max_depth);
          // Only send OnFocus when we change.
          if (focused != closest && closest->callbacks.onfocus) {
            closest->callbacks.onfocus(closest);
          }
          focused = closest;
          if (focused->callbacks.onclick) {
            handled = focused->callbacks.onclick(focused, &e->button);
          }
        }
      }
    } else if (e->type == QB_MOUSE_EVENT_SCROLL) {
      if (closest && closest->callbacks.onscroll) {
        handled = closest->callbacks.onscroll(closest, &e->scroll);
      }
    }
    return handled;
  });
}

qbRenderPass gui_create_renderpass(uint32_t width, uint32_t height) {
  qbBufferBinding_ binding = {};
  binding.binding = 0;
  binding.stride = 8 * sizeof(float);
  binding.input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
   
  qbVertexAttribute_ attributes[3] = {};
  {
    // Position
    qbVertexAttribute_* attr = attributes;
    attr->binding = 0;
    attr->location = 0;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)0;
  }
  {
    // Color
    qbVertexAttribute_* attr = attributes + 1;
    attr->binding = 0;
    attr->location = 1;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(3 * sizeof(float));
  }
  {
    // UVs
    qbVertexAttribute_* attr = attributes + 2;
    attr->binding = 0;
    attr->location = 2;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(6 * sizeof(float));
  }

  qbShaderModule shader_module;
  {
    qbShaderResourceInfo_ resources[3] = {};
    {
      qbShaderResourceInfo info = resources;
      info->binding = GuiUniformCamera::Binding();
      info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
      info->stages = QB_SHADER_STAGE_VERTEX;
      info->name = "Camera";
    }
    {
      qbShaderResourceInfo info = resources + 1;
      info->binding = GuiUniformModel::Binding();
      info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
      info->stages = QB_SHADER_STAGE_VERTEX;
      info->name = "Model";
    }
    {
      qbShaderResourceInfo attr = resources + 2;
      attr->binding = 2;
      attr->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      attr->stages = QB_SHADER_STAGE_FRAGMENT;
      attr->name = "tex_sampler";
    }

    qbShaderModuleAttr_ attr = {};
    attr.vs = "resources/gui.vs";
    attr.fs = "resources/gui.fs";

    attr.resources = resources;
    attr.resources_count = sizeof(resources) / sizeof(resources[0]);

    qb_shadermodule_create(&shader_module, &attr);
  }
  {
    qbGpuBuffer camera_ubo;
    {
      qbGpuBufferAttr_ attr = {};
      attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
      attr.data = nullptr;
      attr.size = sizeof(GuiUniformCamera);

      qb_gpubuffer_create(&camera_ubo, &attr);
    }

    uint32_t bindings[] = { GuiUniformCamera::Binding() };
    qbGpuBuffer ubo_buffers[] = { camera_ubo };
    qb_shadermodule_attachuniforms(shader_module, 1, bindings, ubo_buffers);

    {
      GuiUniformCamera camera;
      camera.projection = glms_ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
      qb_gpubuffer_update(camera_ubo, 0, sizeof(GuiUniformCamera), &camera.projection);
    }
  }
  {
    qbImageSampler image_samplers[1];
    {
      qbImageSamplerAttr_ attr = {};
      attr.image_type = QB_IMAGE_TYPE_2D;
      attr.min_filter = QB_FILTER_TYPE_LINEAR;
      attr.mag_filter = QB_FILTER_TYPE_LINEAR;
      attr.s_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      attr.t_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      qb_imagesampler_create(image_samplers, &attr);
    }

    uint32_t bindings[] = { 2 };
    qbImageSampler* samplers = image_samplers;
    qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
  }

  {
    qbRenderPassAttr_ attr = {};
    attr.frame_buffer = nullptr;
    attr.name = "Gui Render Pass";
    attr.supported_geometry.bindings = &binding;
    attr.supported_geometry.bindings_count = 1;
    attr.supported_geometry.attributes = attributes;
    attr.supported_geometry.attributes_count = 3;
    attr.shader = shader_module;
    attr.viewport = { 0.0, 0.0, (float)width, (float)height };
    attr.viewport_scale = 1.0f;

    qbClearValue_ clear;
    clear.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
    clear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clear.depth = 1.0f;
    attr.clear = clear;

    qb_renderpass_create(&gui_render_pass, &attr);
  }

  {
    float vertices[] = {
     // Positions        // Colors          // UVs
      0.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
      0.0f, 1.0f, 0.0f,   1.0f, 0.0f, 1.0f,  0.0f, 1.0f,
    };

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = vertices;
    attr.size = sizeof(vertices);
    attr.elem_size = sizeof(vertices[0]);

    qb_gpubuffer_create(&window_vbo, &attr);
  }
  {
    int indices[] = {
      3, 1, 0,
      3, 2, 1
    };

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = indices;
    attr.size = sizeof(indices);
    attr.elem_size = sizeof(indices[0]);
    qb_gpubuffer_create(&window_ebo, &attr);
  }
  {
    qbRenderGroupAttr_ attr = {};
    qb_rendergroup_create(&gui_render_group, &attr);
    qb_renderpass_append(gui_render_pass, gui_render_group);
  }

  return gui_render_pass;
}

void qb_window_create_(qbWindow* window_ref, vec3s pos, vec2s size, bool open,
                       qbWindowCallbacks callbacks, qbWindow parent, qbImage background,
                       vec4s color, qbGpuBuffer ubo, qbMeshBuffer dbo, GuiRenderMode render_mode) {
  qbWindow window = *window_ref = new qbWindow_;
  window->id = window_id++;
  window->parent = parent;
  window->rel_pos = pos;// glm::vec3{ pos.x, pos.y, 0.0f };
  window->pos = window->parent
    ? glms_vec3_add(window->parent->pos, window->rel_pos)
    : window->rel_pos;
  window->size = size;
  window->dirty = true;
  window->background = background;
  window->color = color;
  window->render_mode = render_mode;
  if (callbacks) {
    window->callbacks = *callbacks;
  } else {
    window->callbacks = {};
  }
  window->ubo = ubo;
  window->dbo = dbo;

  closed_windows.push_back(window);

  if (open) {
    qb_window_open(window);
  }
}

void qb_window_create(qbWindow* window, qbWindowAttr attr, vec2s pos, vec2s size, qbWindow parent, bool open) {
  qbGpuBuffer ubo;
  qbMeshBuffer dbo;
  {
    qbGpuBufferAttr_ buffer_attr = {};
    buffer_attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    buffer_attr.size = sizeof(GuiUniformModel);
    qb_gpubuffer_create(&ubo, &buffer_attr);
  }
  {
    qbMeshBufferAttr_ buffer_attr = {};
    buffer_attr.descriptor = *qb_renderpass_supportedgeometry(gui_render_pass);

    qb_meshbuffer_create(&dbo, &buffer_attr);
    qb_rendergroup_append(gui_render_group, dbo);

    qbGpuBuffer vertex_buffers[] = { window_vbo };
    qb_meshbuffer_attachvertices(dbo, vertex_buffers);
    qb_meshbuffer_attachindices(dbo, window_ebo);

    uint32_t bindings[] = { GuiUniformModel::Binding() };
    qbGpuBuffer uniform_buffers[] = { ubo };
    qb_meshbuffer_attachuniforms(dbo, 1, bindings, uniform_buffers);

    if (attr->background) {
      uint32_t image_bindings[] = { 2 };
      qbImage images[] = { attr->background };
      qb_meshbuffer_attachimages(dbo, 1, image_bindings, images);
    }
  }

  GuiRenderMode render_mode = attr->background
    ? GUI_RENDER_MODE_IMAGE
    : GUI_RENDER_MODE_SOLID;
  qb_window_create_(window, vec3s{ pos.x, pos.y, 0.0f }, size, open, attr->callbacks, parent, attr->background, attr->background_color, ubo, dbo, render_mode);
  (*window)->scale = { 1.0, 1.0 };
}

void qb_textbox_createtext(qbTextAlign align, size_t font_size, vec2s size, vec2s scale, const char16_t* text, std::vector<float>* vertices, std::vector<int>* indices, qbImage* atlas) {
  Font* font = font_registry->Get(kDefaultFont, (uint32_t)font_size);
  FontRender renderer(font);
  renderer.Render(text, align, size, scale, vertices, indices);
  *atlas = font->Atlas();
}


// Todo: improve with decomposed signed distance fields: 
// https://gamedev.stackexchange.com/questions/150704/freetype-create-signed-distance-field-based-font
void qb_textbox_create(qbWindow* window,
                       qbTextboxAttr textbox_attr,
                       vec2s pos, vec2s size, qbWindow parent, bool open,
                       uint32_t font_size,
                       const char16_t* text) {

  std::vector<float> vertices;
  std::vector<int> indices;
  qbImage font_atlas;
  qb_textbox_createtext(textbox_attr->align, font_size, size, { 1.0f, 1.0f }, text, &vertices, &indices, &font_atlas);

  qbGpuBuffer ubo, ebo, vbo;
  qbMeshBuffer dbo;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(GuiUniformModel);
    qb_gpubuffer_create(&ubo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = vertices.data();
    attr.size = vertices.size() * sizeof(float);
    attr.elem_size = sizeof(float);

    qb_gpubuffer_create(&vbo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = indices.data();
    attr.size = indices.size() * sizeof(int);
    attr.elem_size = sizeof(int);

    qb_gpubuffer_create(&ebo, &attr);
  }

  {
    qbMeshBufferAttr_ attr = {};
    attr.descriptor = *qb_renderpass_supportedgeometry(gui_render_pass);

    qb_meshbuffer_create(&dbo, &attr);
    qb_rendergroup_append(gui_render_group, dbo);

    qbGpuBuffer vertex_buffers[] = { vbo };
    qb_meshbuffer_attachvertices(dbo, vertex_buffers);
    qb_meshbuffer_attachindices(dbo, ebo);

    uint32_t bindings[] = { GuiUniformModel::Binding() };
    qbGpuBuffer uniform_buffers[] = { ubo };
    qb_meshbuffer_attachuniforms(dbo, 1, bindings, uniform_buffers);

    uint32_t image_bindings[] = { 2 };
    qbImage images[] = { font_atlas };
    qb_meshbuffer_attachimages(dbo, 1, image_bindings, images);
  }

  qb_window_create_(window, vec3s{ pos.x, pos.y, 0.0f }, size,
                    open, nullptr,
                    parent, nullptr, textbox_attr->text_color,
                    ubo, dbo, GUI_RENDER_MODE_STRING);
  (*window)->scale = { 1.0f, 1.0f };
  (*window)->text_color = textbox_attr->text_color;
  (*window)->align = textbox_attr->align;
  (*window)->font_size = font_size;
}

void qb_textbox_text(qbWindow window, const char16_t* text) {
  std::vector<float> vertices;
  std::vector<int> indices;
  qbImage font_atlas;
  qb_textbox_createtext(window->align, window->font_size, window->size, window->scale,
                        text, &vertices, &indices, &font_atlas);

  qbGpuBuffer vbo, ebo;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = vertices.data();
    attr.size = vertices.size() * sizeof(float);
    attr.elem_size = sizeof(float);

    qb_gpubuffer_create(&vbo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = indices.data();
    attr.size = indices.size() * sizeof(int);
    attr.elem_size = sizeof(int);

    qb_gpubuffer_create(&ebo, &attr);
  }

  qbGpuBuffer* dst_vbos;
  qb_meshbuffer_vertices(window->dbo, &dst_vbos);
  qbGpuBuffer old = dst_vbos[0];
  dst_vbos[0] = vbo;
  qb_meshbuffer_attachvertices(window->dbo, dst_vbos);
  qb_gpubuffer_destroy(&old);
  
  qbGpuBuffer dst_ebo;
  qb_meshbuffer_indices(window->dbo, &dst_ebo);
  qb_meshbuffer_attachindices(window->dbo, ebo);
  qb_gpubuffer_destroy(&dst_ebo);
}

void qb_textbox_color(qbWindow window, vec4s text_color) {
  window->text_color = text_color;
}

void qb_textbox_scale(qbWindow window, vec2s scale) {
  window->scale = scale;
}

void qb_textbox_fontsize(qbWindow window, uint32_t font_size) {
  window->font_size = font_size;
}

void qb_window_open(qbWindow window) {
  auto found = std::find(closed_windows.begin(), closed_windows.end(), window);
  if (found != closed_windows.end()) {
    std::vector<qbWindow> children;
    for (qbWindow w : closed_windows) {
      if (w->parent == window) {
        children.push_back(w);
      }
    }

    InsertWindow(window);
    closed_windows.erase(found);

    for (qbWindow child : children) {
      qb_window_open(child);
    }
  }
}

void qb_window_close(qbWindow window) {
  auto found = std::find(windows.begin(), windows.end(), window);
  if (found != windows.end()) {
    std::vector<qbWindow> children;
    for (qbWindow w : windows) {
      if (w->parent == window) {
        children.push_back(w);
      }
    }

    for (qbWindow child : children) {
      qb_window_close(child);
    }

    EraseWindow(window);
    closed_windows.push_back(window);
    if (window == focused) {
      focused = nullptr;
    }
  }
}

void qb_window_updateuniform(qbWindow window) {
  mat4s model = GLMS_MAT4_IDENTITY_INIT;

  model = glms_translate(model, window->pos);

  if (window->render_mode == GUI_RENDER_MODE_STRING) {
    model = glms_scale(model, vec3s{ window->scale.x, window->scale.y, 1.0f });
  } else {
    model = glms_scale(model, vec3s{ window->size.x, window->size.y, 1.0f });
  }

  window->uniform.modelview = model;
  window->uniform.color = window->color;
  window->uniform.render_mode = window->render_mode;
  qb_gpubuffer_update(window->ubo, 0, sizeof(GuiUniformModel), &window->uniform);
}

void gui_window_updateuniforms() {
  float depth = 1.0f;
  for (auto& window : windows) {
    if (window->dirty || (window->parent && window->parent->dirty)) {
      // If the parent is dirty, then this will update the position properly.
      window->pos = window->rel_pos;
      if (window->parent) {
        window->pos = glms_vec3_add(window->parent->pos, window->rel_pos);
      }
    }
    window->rel_pos.z = 0;
    window->pos.z = depth;
    depth -= 0.0001f;
    qb_window_updateuniform(window);
  }
  for (auto& window : windows) {
    window->dirty = false;
  }
}

void qb_window_movetofront(qbWindow window) {
  MoveToFront(window);
  std::vector<qbMeshBuffer> buffers;
  buffers.reserve(windows.size());
  for (qbWindow window : windows) {
    buffers.push_back(window->dbo);
  }
  std::reverse(buffers.begin(), buffers.end());
  qb_rendergroup_update(gui_render_group, buffers.size(), buffers.data());
}

void qb_window_movetoback(qbWindow window) {
  MoveToBack(window);
  std::vector<qbMeshBuffer> buffers;
  buffers.reserve(windows.size());
  for (qbWindow window : windows) {
    buffers.push_back(window->dbo);
  }
  std::reverse(buffers.begin(), buffers.end());
  qb_rendergroup_update(gui_render_group, buffers.size(), buffers.data());
}

void qb_window_moveforward(qbWindow window) {
  if (windows.size() > 1) {
    auto& it = std::find(windows.begin(), windows.end(), window);
    if (it == windows.begin()) {
      return;
    }
    auto& prev = --it;

    std::swap(*it, *prev);
  }
}

void qb_window_movebackward(qbWindow window) {
  if (windows.size() > 1) {
    auto& it = std::find(windows.begin(), windows.end(), window);
    if (it == windows.end()) {
      return;
    }
    auto& next = ++it;
    if (next == windows.end()) {
      return;
    }

    std::swap(*it, *next);
  }
}

void qb_window_moveto(qbWindow window, vec3s pos) {
  window->pos = pos;
  window->pos.z = glm_clamp(window->pos.z, -0.9999f, 0.9999f);
  window->rel_pos = window->parent
    ? glms_vec3_sub(window->parent->pos, window->rel_pos)
    : window->rel_pos;
  window->dirty = true;
}

void qb_window_resizeto(qbWindow window, vec2s size) {
  window->size = size;
  window->dirty = true;
}

void qb_window_moveby(qbWindow window, vec3s pos_delta) {
  window->rel_pos = glms_vec3_add(window->rel_pos, pos_delta);
  window->rel_pos.z = glm_clamp(window->rel_pos.z, -0.9999f, 0.9999f);
  window->pos = window->parent
    ? glms_vec3_add(window->parent->pos, window->rel_pos)
    : window->rel_pos;
  window->dirty = true;
}

void qb_window_resizeby(qbWindow window, vec2s size_delta) {
  window->size = glms_vec2_add(window->size, size_delta);
  window->dirty = true;
}

qbWindow qb_window_focus() {
  return focused;
}

qbWindow qb_window_focusat(int x, int y) {
  return FindClosestWindow(x, y);
}

vec2s qb_window_size(qbWindow window) {
  return window->size;
}

vec2s qb_window_pos(qbWindow window) {
  return { window->pos.x, window->pos.y };
}

vec2s qb_window_relpos(qbWindow window) {
  return { window->rel_pos.x, window->rel_pos.y };
}

qbWindow qb_window_parent(qbWindow window) {
  return window->parent;
}