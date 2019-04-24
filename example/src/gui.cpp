#include <cubez/cubez.h>
#include "render_module.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <Ultralight/Renderer.h>
#include <Ultralight/View.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/GPUDriver.h>

#ifdef __COMPILE_AS_WINDOWS__
#define FRAMEWORK_PLATFORM_WIN
#include <Framework/platform/win/FileSystemWin.h>
#include <Framework/platform/win/FontLoaderWin.h>
#elif defined(__COMPILE_AS_LINUX__)
#define FRAMEWORK_PLATFORM_LINUX
#include <Framework/platform/common/FileSystemBasic.h>
#include <Framework/platform/common/FontLoaderRoboto.h>
#endif
#include <Framework/Platform.h>
#include <Framework/Overlay.h>

#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include "gui.h"
#include "empty_overlay.h"
#include <atomic>
#include <iostream>
#include <glm/ext.hpp>
#include <set>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace gui
{

// std140 layout
struct GuiUniformCamera {
  alignas(16) glm::mat4 projection;
  
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
  alignas(16) glm::mat4 modelview;
  alignas(16) glm::vec4 color;
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
  glm::vec3 pos;

  // Relative position to the parent. If there is no parent, this is relative
  // to the top-left corner, i.e. absolute position.
  glm::vec3 rel_pos;
  glm::vec2 size;

  GuiUniformModel uniform;
  qbGpuBuffer ubo;
  qbMeshBuffer dbo;

  GuiRenderMode render_mode;

  // True if there is a need to update the buffers.
  bool dirty;

  glm::vec4 color;
  glm::vec4 text_color;
  glm::vec2 scale;
  qbImage background;
  qbWindowCallbacks_ callbacks;
};

struct TextBox {
  qbWindow_ window;

  const char16_t* text;
  glm::vec4 color;
  glm::vec2 scale;
};

Context context;
SDL_Window* win;
qbRenderPass gui_render_pass;
qbGpuBuffer window_vbo;
qbGpuBuffer window_ebo;

std::atomic_uint window_id;
std::vector<qbWindow> windows;
qbWindow focused;

struct Character {
  float ax; // advance.x
  float ay; // advance.y

  float bw; // bitmap.width;
  float bh; // bitmap.rows;

  float bl; // bitmap_left;
  float bt; // bitmap_top;

  float tx; // x offset of glyph in texture coordinates
};
std::map<char16_t, Character> characters;
qbImage font_atlas;
uint32_t atlas_width;
uint32_t atlas_height;
uint32_t font_height;

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

qbWindow FindClosestWindow(glm::ivec2 mouse) {
  if (windows.empty()) {
    return nullptr;
  }
  qbWindow closest = nullptr;
  for (qbWindow window : windows) {
    glm::vec3 window_pos = window->pos;
    if (mouse.x >= window_pos.x &&
        mouse.x < window_pos.x + window->size.x &&
        mouse.y >= window_pos.y &&
        mouse.y < window_pos.y + window->size.y) {
      closest = window;
      break;
    }
  }
  return closest;
}

qbWindow FindFarthestWindow(glm::ivec2 mouse) {
  if (windows.empty()) {
    return nullptr;
  }
  qbWindow farthest = nullptr;
  for (qbWindow window : windows) {
    glm::vec3 window_pos = window->pos;
    if (mouse.x >= window_pos.x &&
        mouse.x < window_pos.x + window->size.x &&
        mouse.y >= window_pos.y &&
        mouse.y < window_pos.y + window->size.y) {
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

void BringToFront(qbWindow window) {
  if (windows.size() > 1) {
    auto& it = std::find(windows.begin(), windows.end(), window);
    std::swap(*it, *windows.begin());
  }
}

void SendToBack(qbWindow window) {
  if (windows.size() > 1) {
    auto& it = std::find(windows.begin(), windows.end(), window);
    std::swap(*it, *(windows.end() - 1));
  }
}

}

qbPixelAlignmentOglExt_ alignment_ext;

// https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02#Creating_a_texture_atlas
void CreateFontAtlas() {
  FT_Library library;
  FT_Face face;

  {
    auto error = FT_Init_FreeType(&library);
    if (error) {
      FATAL("Could not initialize FreeType");
    }

    const char* file = "resources/fonts/OpenSans-Regular.ttf";
    error = FT_New_Face(library,
                        file,
                        0,
                        &face);
    if (error == FT_Err_Unknown_File_Format) {
      FATAL("Unknown file format");
    } else if (error) {
      FATAL("Could not open font");
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
  }

  FT_GlyphSlot g = face->glyph;

  // Converts 1/64th units to pixel units.
  font_height = face->size->metrics.height >> 6;
  //font_height = face->height >> 6;
  unsigned w = 0;
  unsigned h = 0;

  for (int i = 32; i < 128; i++) {
    if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
      fprintf(stderr, "Loading character %c failed!\n", i);
      continue;
    }

    w += g->bitmap.width;
    h = std::max(h, g->bitmap.rows);
  }

  /* you might as well save this value as it is needed later on */
  atlas_width = w;
  atlas_height = h;
  qbImageAttr_ atlas = {};
  atlas.name = "font atlas";
  atlas.type = QB_IMAGE_TYPE_2D;
  atlas.ext = (qbRenderExt)&alignment_ext;
  qb_image_raw(&font_atlas, &atlas, QB_PIXEL_FORMAT_R8, w, h, nullptr);

  int x = 0;

  for (int i = 32; i < 128; i++) {
    if (FT_Load_Char(face, i, FT_LOAD_RENDER))
      continue;
    Character c = {};
    c.ax = g->advance.x >> 6;
    c.ay = g->advance.y >> 6;
    c.bw = g->bitmap.width;
    c.bh = g->bitmap.rows;
    c.bl = g->bitmap_left;
    c.bt = g->bitmap_top;
    c.tx = (float)x / w;
    characters[i] = c;

    qb_image_update(font_atlas, { x, 0, 0 }, { g->bitmap.width, g->bitmap.rows, 0 }, g->bitmap.buffer);
    x += g->bitmap.width;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(library);
}

void InitializeRenderExts() {
  alignment_ext.ext = {};
  alignment_ext.ext.name = "qbPixelAlignmentOglExt_";
  alignment_ext.alignment = 1;
}

void Initialize() {
  InitializeRenderExts();
  CreateFontAtlas();
}

void Shutdown() {
}

void HandleInput(SDL_Event* e) {
  glm::ivec2 pos;
  input::get_mouse_position(&pos.x, &pos.y);
  qbWindow max_depth = FindClosestWindow(pos);

  if (e->type == SDL_MOUSEBUTTONDOWN) {
    if (max_depth) {
      qb_window_movetofront(max_depth);
      // Only send OnFocus when we change.
      if (focused != max_depth && max_depth->callbacks.onfocus) {
        max_depth->callbacks.onfocus(max_depth);
      }
      focused = max_depth;
      if (focused->callbacks.onclick) {
        input::MouseEvent mouse_e;
        mouse_e.event_type = input::QB_MOUSE_EVENT_BUTTON;
        if (e->button.button == SDL_BUTTON_LEFT) {
          mouse_e.button_event.mouse_button = QB_BUTTON_LEFT;
        } else if (e->button.button == SDL_BUTTON_MIDDLE) {
          mouse_e.button_event.mouse_button = QB_BUTTON_MIDDLE;
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
          mouse_e.button_event.mouse_button = QB_BUTTON_RIGHT;
        } else if (e->button.button == SDL_BUTTON_X1) {
          mouse_e.button_event.mouse_button = QB_BUTTON_X1;
        } else if (e->button.button == SDL_BUTTON_X2) {
          mouse_e.button_event.mouse_button = QB_BUTTON_X2;
        }
        mouse_e.button_event.state = e->button.state;
        focused->callbacks.onclick(focused, &mouse_e);
      }
      std::cout << "( " << (e->button.x - focused->pos.x) << ", "
        << (e->button.y - focused->pos.y) << " )\n";
    }
  } else if (e->type == SDL_MOUSEBUTTONUP) {
    if (focused && focused->callbacks.onclick) {
      input::MouseEvent mouse_e;
      mouse_e.event_type = input::QB_MOUSE_EVENT_BUTTON;
      if (e->button.button == SDL_BUTTON_LEFT) {
        mouse_e.button_event.mouse_button = QB_BUTTON_LEFT;
      } else if (e->button.button == SDL_BUTTON_MIDDLE) {
        mouse_e.button_event.mouse_button = QB_BUTTON_MIDDLE;
      } else if (e->button.button == SDL_BUTTON_RIGHT) {
        mouse_e.button_event.mouse_button = QB_BUTTON_RIGHT;
      } else if (e->button.button == SDL_BUTTON_X1) {
        mouse_e.button_event.mouse_button = QB_BUTTON_X1;
      } else if (e->button.button == SDL_BUTTON_X2) {
        mouse_e.button_event.mouse_button = QB_BUTTON_X2;
      }
      mouse_e.button_event.state = e->button.state;
      focused->callbacks.onclick(focused, &mouse_e);
    }
  } else if (e->type == SDL_MOUSEMOTION) {
    if (focused) {
      //qb_window_moveto(focused, { e->motion.x, e->motion.y, 0.0f });
    }
  } else if (e->type == SDL_MOUSEWHEEL) {
    if (max_depth && max_depth->callbacks.onclick) {
      input::MouseEvent mouse_e;
      mouse_e.event_type = input::QB_MOUSE_EVENT_SCROLL;
      mouse_e.scroll_event.x = e->wheel.x;
      mouse_e.scroll_event.y = e->wheel.y;
      max_depth->callbacks.onscroll(max_depth, &mouse_e);
    }
  }
}

qbRenderPass CreateGuiRenderPass(qbFrameBuffer frame, uint32_t width, uint32_t height) {
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
      camera.projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
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
      attr.s_wrap = QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
      attr.t_wrap = QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
      qb_imagesampler_create(image_samplers, &attr);
    }

    uint32_t bindings[] = { 2 };
    qbImageSampler* samplers = image_samplers;
    qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
  }

  {
    qbRenderPassAttr_ attr = {};
    attr.frame_buffer = frame;

    attr.bindings = &binding;
    attr.bindings_count = 1;

    attr.attributes = attributes;
    attr.attributes_count = 3;

    attr.shader = shader_module;

    attr.viewport = { 0.0, 0.0, width, height };
    attr.viewport_scale = 1.0f;

    qbClearValue_ clear;
    clear.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
    clear.color = { 0.0, 0.0, 0.0, 1.0 };
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

  return gui_render_pass;
}

void qb_window_create_(qbWindow* window_ref, glm::vec3 pos, glm::vec2 size, bool open,
                       qbWindowCallbacks callbacks, qbWindow parent, qbImage background,
                       glm::vec4 color, qbGpuBuffer ubo, qbMeshBuffer dbo, GuiRenderMode render_mode) {
  qbWindow window = *window_ref = new qbWindow_;
  window->id = window_id++;
  window->parent = parent;
  window->rel_pos = pos;// glm::vec3{ pos.x, pos.y, 0.0f };
  window->pos = window->parent
    ? window->parent->pos + window->rel_pos
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

void qb_window_create(qbWindow* window_ref, glm::vec3 pos, glm::vec2 size, bool open,
                      qbWindowCallbacks callbacks, qbWindow parent, qbImage background,
                      glm::vec4 color) {
  qbGpuBuffer ubo;
  qbMeshBuffer dbo;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(GuiUniformModel);
    qb_gpubuffer_create(&ubo, &attr);
  }
  {
    qbMeshBufferAttr_ attr = {};
    attr.attributes_count = qb_renderpass_attributes(gui_render_pass, &attr.attributes);
    attr.bindings_count = qb_renderpass_bindings(gui_render_pass, &attr.bindings);

    qb_meshbuffer_create(&dbo, &attr);
    qb_renderpass_append(gui_render_pass, dbo);

    qbGpuBuffer vertex_buffers[] = { window_vbo };
    qb_meshbuffer_attachvertices(dbo, vertex_buffers);
    qb_meshbuffer_attachindices(dbo, window_ebo);

    uint32_t bindings[] = { GuiUniformModel::Binding() };
    qbGpuBuffer uniform_buffers[] = { ubo };
    qb_meshbuffer_attachuniforms(dbo, 1, bindings, uniform_buffers);

    if (background) {
      uint32_t image_bindings[] = { 2 };
      qbImage images[] = { background };
      qb_meshbuffer_attachimages(dbo, 1, image_bindings, images);
    }
  }

  GuiRenderMode render_mode = background
    ? GUI_RENDER_MODE_IMAGE
    : GUI_RENDER_MODE_SOLID;
  qb_window_create_(window_ref, pos, size, open, callbacks, parent, background, color, ubo, dbo, render_mode);
}

void qb_textbox_createtext(glm::vec2 scale, const char16_t* text, std::vector<float>* vertices, std::vector<int>* indices) {
  if (!text) {
    return;
  }

  size_t len = 0;
  for (size_t i = 0; i < 1 << 10; ++i) {
    if (text[i] == 0) {
      break;
    }
    ++len;
  }

  float x = 0;
  float y = 0;
  int index = 0;
  int line = 0;
  for (size_t i = 0; i < len; ++i) {
    Character& c = characters[text[i]];
    if (text[i] == '\n') {
      x = 0;
      y -= font_height;
      continue;
    }
    if (text[i] == ' ') {
      x += c.ax * scale.x;
      continue;
    }

    float l = x + c.bl * scale.x;
    float t = -y - c.bt * scale.y + atlas_height;
    float w = c.bw * scale.x;
    float h = c.bh * scale.y;
    float r = l + w;
    float b = t + h;
    float txl = c.tx;
    float txr = c.tx + c.bw / atlas_width;
    float txt = 0.0f;
    float txb = c.bh / atlas_height;

    if (!w || !h) {
      continue;
    }

    float verts[] = {
      // Positions        // Colors          // UVs
      l, t, 0.0f,   1.0f, 0.0f, 0.0f,  txl, txt,
      r, t, 0.0f,   0.0f, 1.0f, 0.0f,  txr, txt,
      r, b, 0.0f,   0.0f, 0.0f, 1.0f,  txr, txb,
      l, b, 0.0f,   1.0f, 0.0f, 1.0f,  txl, txb,
    };
    vertices->insert(vertices->end(), verts, verts + (sizeof(verts) / sizeof(float)));

    int indx[] = {
      index + 3, index + 1, index + 0,
      index + 3, index + 2, index + 1
    };
    indices->insert(indices->end(), indx, indx + (sizeof(indx) / sizeof(int)));
    index += 4;
    /* Advance the cursor to the start of the next character */
    x += c.ax * scale.x;
    y += c.ay * scale.y;
  }
}

void qb_textbox_create(qbWindow* window, glm::vec3 pos, glm::vec2 size, glm::vec2 scale, bool open,
                       qbWindowCallbacks callbacks, qbWindow parent, glm::vec4 text_color, const char16_t* text) {


  std::vector<float> vertices;
  std::vector<int> indices;
  qb_textbox_createtext(scale, text, &vertices, &indices);

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
    attr.attributes_count = qb_renderpass_attributes(gui_render_pass, &attr.attributes);
    attr.bindings_count = qb_renderpass_bindings(gui_render_pass, &attr.bindings);

    qb_meshbuffer_create(&dbo, &attr);
    qb_renderpass_append(gui_render_pass, dbo);

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

  qb_window_create_(window, pos, size, open, callbacks, parent, nullptr, text_color, ubo, dbo, GUI_RENDER_MODE_STRING);
  (*window)->scale = scale;
  (*window)->text_color = text_color;
}

void qb_textbox_text(qbWindow window, const char16_t* text) {
  std::vector<float> vertices;
  std::vector<int> indices;
  qb_textbox_createtext(window->scale, text, &vertices, &indices);

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

void qb_textbox_text(qbWindow window, const char16_t* text);
void qb_textbox_color(qbWindow window, glm::vec4 text_color);
void qb_textbox_scaleto(qbWindow window, glm::vec2 scale);
void qb_textbox_scaleby(qbWindow window, glm::vec2 scale_delta);

void qb_window_open(qbWindow window) {
  auto found = std::find(closed_windows.begin(), closed_windows.end(), window);
  if (found != closed_windows.end()) {
    InsertWindow(window);
    closed_windows.erase(found);
  }
}

void qb_window_close(qbWindow window) {
  auto found = std::find(windows.begin(), windows.end(), window);
  if (found != windows.end()) {
    EraseWindow(window);
    closed_windows.push_back(window);
  }
}

void qb_window_updateuniform(qbWindow window) {
  glm::mat4 model = glm::mat4(1.0);  
  //model = glm::rotate(model, 3.1415f, glm::vec3(0.0f, 0.0f, 1.0f));
  model = glm::translate(model, glm::vec3(window->pos));
  model = glm::scale(model, glm::vec3(window->size, 1.0f));
  //model = glm::translate(model, glm::vec3(0.5f * window->size.x, 0.5f * window->size.y, 0.0f));
  //model = glm::translate(model, glm::vec3(-0.5f * window->size.x, -0.5f * window->size.y, 0.0f));

  window->uniform.modelview = model;
  window->uniform.color = window->color;
  window->uniform.render_mode = window->render_mode;
  qb_gpubuffer_update(window->ubo, 0, sizeof(GuiUniformModel), &window->uniform.modelview);
}

void qb_window_updateuniforms() {
  float depth = 1.0f;
  for (auto& window : windows) {
    if (window->dirty || (window->parent && window->parent->dirty)) {
      // If the parent is dirty, then this will update the position properly.
      window->pos = window->parent
        ? window->parent->pos + window->rel_pos
        : window->rel_pos;
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
  BringToFront(window);
  std::vector<qbMeshBuffer> buffers;
  buffers.reserve(windows.size());
  for (qbWindow window : windows) {
    buffers.push_back(window->dbo);    
  }
  std::reverse(buffers.begin(), buffers.end());
  qb_renderpass_update(gui_render_pass, buffers.size(), buffers.data());
}

void qb_window_movetoback(qbWindow window) {
  BringToFront(window);
  std::vector<qbMeshBuffer> buffers;
  buffers.reserve(windows.size());
  for (qbWindow window : windows) {
    buffers.push_back(window->dbo);
  }
  std::reverse(buffers.begin(), buffers.end());
  qb_renderpass_update(gui_render_pass, buffers.size(), buffers.data());
}

void qb_window_moveforward(qbWindow window);
void qb_window_movebackward(qbWindow window);

void qb_window_moveto(qbWindow window, glm::vec3 pos) {
  window->pos = pos;
  window->pos.z = glm::clamp(window->rel_pos.z, -0.9999f, 0.9999f);
  window->rel_pos = window->parent
    ? window->parent->pos - window->rel_pos
    : window->rel_pos;
  window->dirty = true;
}

void qb_window_resizeto(qbWindow window, glm::vec2 size) {
  window->size = size;
  window->dirty = true;
}

void qb_window_moveby(qbWindow window, glm::vec3 pos_delta) {
  window->rel_pos += pos_delta;
  window->rel_pos.z = glm::clamp(window->rel_pos.z, -0.9999f, 0.9999f);
  window->pos = window->parent
    ? window->parent->pos + window->rel_pos
    : window->rel_pos;
  window->dirty = true;
}

void qb_window_resizeby(qbWindow window, glm::vec2 size_delta) {
  window->size += size_delta;
  window->dirty = true;
}

}
