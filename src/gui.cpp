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

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <assert.h>
#include <algorithm>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <cubez/gui.h>
#include "gui_internal.h"
#include <atomic>
#include <iostream>
#include <set>
#include <list>
#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "font_registry.h"
#include "font_render.h"
#include <cglm/struct.h>
#include <filesystem>
#include "inline_shaders.h"

namespace fs = std::filesystem;

// std140 layout
struct GuiUniformCamera {
  alignas(16) mat4s projection;
  
  static uint32_t Binding() {
    return 0;
  }
};

struct qbConstraint_ {
  qbConstraint c;
  float v;
};

enum GuiRenderMode {
  GUI_RENDER_MODE_SOLID,
  GUI_RENDER_MODE_IMAGE,
  GUI_RENDER_MODE_STRING,
};

// std140 layout
struct GuiUniformModel {
  mat4s modelview;
  vec4s color;  
  GuiRenderMode render_mode;
  float radius;
  vec2s size;

  static uint32_t Binding() {
    return 1;
  }
};

struct qbGuiElement_ {
  std::string name;
  uint32_t id;
  qbGuiElement parent;

  // X, Y are positional coordinates.
  // pos = parent->pos + rel_pos
  vec2s pos;

  // Relative position to the parent. If there is no parent, this is relative
  // to the top-left corner, i.e. absolute position.
  vec2s rel_pos;
  vec2s size;

  GuiUniformModel uniform;
  qbGpuBuffer ubo;
  qbMeshBuffer dbo;

  qbGpuBuffer text_ubo;
  qbMeshBuffer text_dbo;

  GuiRenderMode render_mode;

  utf8_t* text;
  size_t text_count;

  // True if there is a need to update the buffers.
  bool dirty;
  bool visible;

  vec4s color;
  vec4s text_color;
  vec2s text_scale;
  uint32_t text_size;
  qbTextAlign text_align;
  std::string font_name;

  qbImage background;
  float radius;
  vec2s offset;

  qbConstraint_ constraints_[(int)QB_GUI_HEIGHT + 1][(int)QB_CONSTRAINT_ASPECT_RATIO + 1];
  qbVar value;
  qbGuiElementCallbacks_ callbacks;
  qbVar state;
};

qbRenderPass gui_render_pass;
qbRenderGroup gui_render_group;
qbGpuBuffer default_vbo;
qbGpuBuffer default_ebo;
qbGpuBuffer camera_ubo;

std::atomic_uint el_id;
qbGuiElement focused;

FontRegistry* font_registry;
const char* kDefaultFont = "opensans";
const int kMaxPathLength = 10;
const size_t kMaxTextBoxGpuBufferSize = 32768;

void qb_guielement_updateuniform(qbGuiElement el, GuiRenderMode render_mode, qbGpuBuffer ubo);
void qb_guielement_destroy_(qbGuiElement el);

fs::path font_directory;

struct GuiNode {
  GuiNode(qbGuiElement el) : el(el) {}

  qbGuiElement el;
  std::vector<std::unique_ptr<GuiNode>> children;

  GuiNode* Find(qbGuiElement el) {
    auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
    return it == children.end() ? nullptr : it->get();
  }

  GuiNode* Find(const std::string& name) {
    auto it = std::find_if(children.begin(), children.end(), [name](const std::unique_ptr<GuiNode>& n) {
      return n->el->name == name;
    });
    return it == children.end() ? nullptr : it->get();
  }

  bool Insert(qbGuiElement el) {
    if (!Find(el)) {
      children.push_back(std::make_unique<GuiNode>(el));
      return true;
    }
    return false;
  }

  bool Insert(std::unique_ptr<GuiNode>&& el) {
    if (!Find(el->el)) {
      children.push_back(std::move(el));
      return true;
    }
    return false;
  }

  bool Erase(qbGuiElement el) {
    auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
    if (it == children.end()) {
      return false;
    }

    children.erase(it);
    return true;
  }

  std::unique_ptr<GuiNode> Release(qbGuiElement el) {
    auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
    std::unique_ptr<GuiNode> node = std::move(*it);
    children.erase(it);
    return node;
  }

  void MoveToFront(qbGuiElement el) {
    auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
    std::swap(*it, children.back());
  }

  void MoveToBack(qbGuiElement el) {
    auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
    std::swap(*it, children.front());
  }

  void MoveForward(qbGuiElement el) {
    if (children.size() > 1) {
      auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
      if (it == children.end() - 1) {
        return;
      }

      std::swap(*it, *(it + 1));
    }
  }

  void MoveBackward(qbGuiElement el) {
    if (children.size() > 1) {
      auto it = std::find_if(children.begin(), children.end(), [el](const auto& n) { return n->el == el; });
      if (it == children.begin()) {
        return;
      }

      std::swap(*it, *(it - 1));
    }
  }
};

class GuiTree {
public:
  GuiTree() : root_(nullptr) {}
  void Insert(qbGuiElement el) {
    if (named_elements_.find(el->name) != named_elements_.end()) {
      return;
    }

    GuiNode* parent_node = &root_;
    if (el->parent) {
      parent_node = FindNode(&root_, el->parent);
    }

    parent_node->Insert(el);
    if (!el->name.empty()) {
      named_elements_[el->name] = parent_node->Find(el);
    }
  }

  void Erase(qbGuiElement el) {
    GuiNode* parent_node = FindNode(&root_, el->parent);
    GuiNode* el_node = parent_node->Find(el);

    for (auto& n : el_node->children) {
      Erase(n->el);
    }

    if (!el->name.empty()) {
      named_elements_.erase(el->name);
    }
    parent_node->Erase(el);
  }

  void Destroy(qbGuiElement el) {
    GuiNode* parent = FindNode(&root_, el->parent);
    GuiNode* node = parent->Find(el);

    // Record what GUI elements to destroy.
    std::vector<qbGuiElement> to_destroy;
    ForEach([this, &to_destroy](qbGuiElement b) {
      to_destroy.push_back(b);
    }, node);

    // Remove from tree.
    Erase(el);

    // Call the destroy callback in reverse topological order.
    for (auto it = to_destroy.rbegin(); it != to_destroy.rend(); ++it) {
      if ((*it)->callbacks.ondestroy) {
        (*it)->callbacks.ondestroy(*it);        
      }
    }

    // Only after all children_map have had a chance to perform their callbacks, actually free the guielement.
    for (auto& el : to_destroy) {
      qb_guielement_destroy_(el);
    }
  }

  void Open(qbGuiElement el) {
    if (!el->visible && el->callbacks.onopen) {
      el->callbacks.onopen(el);
    }
    el->visible = true;
  }

  void Close(qbGuiElement el) {
    if (el->visible && el->callbacks.onclose) {
      el->callbacks.onclose(el);
    }
    el->visible = false;
  }

  void CloseChildren(qbGuiElement el) {
    GuiNode* node = FindNode(&root_, el);
    ForEach([this, el](qbGuiElement e) {
      if (e == el) return;
      Close(e);
    }, node);
  }

  void Move(qbGuiElement el, qbGuiElement new_parent) {
    GuiNode* old_parent_node = FindParent(el);
    GuiNode* new_parent_node = FindNode(&root_, new_parent);

    if (old_parent_node == new_parent_node || !old_parent_node || !new_parent_node) {
      return;
    }

    new_parent_node->Insert(old_parent_node->Release(el));
  }

  qbGuiElement FindClosestBlock(float x, float y) {
    qbGuiElement found = nullptr;
    ForEachBreakoutEarly([x, y, &found](qbGuiElement el) {
      if (x >= el->pos.x + el->offset.x &&
          x < el->pos.x + el->offset.x + el->size.x &&
          y >= el->pos.y + el->offset.y  &&
          y < el->pos.y + el->offset.y + el->size.y) {
        found = el;
        return true;
      }
      return false;
    }, &root_);

    return found;
  }

  qbGuiElement FindClosestBlock(float x, float y, std::function<bool(qbGuiElement)> handler) {
    qbGuiElement found = nullptr;
    ForEachBreakoutEarly([x, y, &found, &handler](qbGuiElement el) {
      if (x >= el->pos.x + el->offset.x &&
          x < el->pos.x + el->offset.x + el->size.x &&
          y >= el->pos.y + el->offset.y  &&
          y < el->pos.y + el->offset.y + el->size.y) {
        found = el;
        if (handler(found)) {
          return true;
        }
      }
      return false;
    }, &root_);

    return found;
  }

  qbGuiElement FindFarthestBlock(float x, float y) {
    qbGuiElement found;
    ForEach([x, y, &found](qbGuiElement el) {
      if (x >= el->pos.x + el->offset.x &&
          x < el->pos.x + el->offset.x + el->size.x &&
          y >= el->pos.y + el->offset.y  &&
          y < el->pos.y + el->offset.y + el->size.y) {
        found = el;
      }
    }, &root_);

    return found;
  }

  void MoveToFront(qbGuiElement el) {
    FindParent(el)->MoveToFront(el);
  }

  void MoveToBack(qbGuiElement el) {
    FindParent(el)->MoveToBack(el);
  }

  void MoveForward(qbGuiElement el) {
    FindParent(el)->MoveForward(el);
  }

  void MoveBackward(qbGuiElement el) {
    FindParent(el)->MoveBackward(el);
  }

  GuiNode* FindParent(qbGuiElement el) {
    if (!el->parent) {
      return &root_;
    }
    return FindNode(&root_, el->parent);
  }

  GuiNode* Find(qbGuiElement el) {
    return FindNode(&root_, el);
  }

  GuiNode* Find(const std::string& name) {
    auto it = named_elements_.find(name);
    if (it == named_elements_.end()) {
      return nullptr;
    }

    return it->second;
  }

  template<class F>
  void ForEach(const F& f) {
    ForEach(f, &root_);
  }

  void UpdateRenderGroup() {
    std::vector<qbMeshBuffer> buffers;
    buffers.reserve(25);

    UpdateRenderGroup(&buffers, &root_);

    qb_rendergroup_update(gui_render_group, buffers.size(), buffers.data());
  }

private:
  void UpdateRenderGroup(std::vector<qbMeshBuffer>* buffers, GuiNode* node) {
    if (node == &root_ || node->el->visible) {
      if (node->el) {
        buffers->push_back(node->el->dbo);
        if (node->el->text_dbo) {
          buffers->push_back(node->el->text_dbo);
        }
      }
      for (auto& c : node->children) {
        UpdateRenderGroup(buffers, c.get());
      }
    }    
  }

  template<class F>
  void ForEach(const F& f, GuiNode* node) {
    if (node->el) {
      f(node->el);
    }

    for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
      ForEach(f, it->get());
    }
  }

  template<class F>
  bool ForEachBreakoutEarly(const F& f, GuiNode* node) {
    
    for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
      auto& n = *it;
      if (ForEachBreakoutEarly(f, n.get())) {
        return true;
      }
    }

    if (node->el) {
      if (f(node->el)) {
        return true;
      }
    }

    return false;
  }

  GuiNode* FindNode(GuiNode* node, qbGuiElement el) {
    if (node->el == el) {
      return node;
    }

    for (auto& n : node->children) {
      GuiNode* found = FindNode(n.get(), el);
      if (found) {
        return found;
      }
    }

    return nullptr;
  }

  GuiNode root_;
  std::unordered_map<std::string, GuiNode*> named_elements_;
};
std::unique_ptr<GuiTree> scene;

void gui_initialize() {
  font_directory = fs::path(qb_resources()->dir) / fs::path(qb_resources()->fonts);
  std::string default_font_file = (font_directory / "OpenSans-Bold.ttf").string();

  font_registry = new FontRegistry();
  if (fs::exists(default_font_file)) {
    font_registry->Load(default_font_file.c_str(), kDefaultFont);
  } else {
    std::cerr << "Could not load default font file: \"" << default_font_file << "\"\n";
  }
  scene = std::make_unique<GuiTree>();
}

void gui_shutdown() {
}

qbBool gui_handle_input(qbInputEvent input_event) {
  int x, y;
  qb_mouse_getposition(&x, &y);

  bool event_handled = false;
  qbGuiElement old_focused = focused;
  qbGuiElement closest = nullptr;

  if (input_event->type == qbInputEventType::QB_INPUT_EVENT_MOUSE) {
    closest = scene->FindClosestBlock(x, y, [input_event](qbGuiElement closest) {
      if (input_event->type == QB_INPUT_EVENT_KEY) {
        return true;
      }

      qbMouseEvent e = &input_event->mouse_event;
      if (e->type == QB_MOUSE_EVENT_BUTTON) {
        if (e->button.button == QB_BUTTON_LEFT && e->button.state == QB_MOUSE_DOWN) {
          focused = closest;
        }
      }

      return true;
    });

    if (old_focused != focused) {
      // Only send OnFocus when we change.
      if (focused->callbacks.onfocus) {
        event_handled |= focused->callbacks.onfocus(focused) == QB_TRUE;
      }

      if (focused->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].c) {
        if (focused->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].c) {
          focused->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v = std::max(
            focused->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v,
            focused->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].v);
        }
        if (focused->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].c) {
          focused->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v = std::min(
            focused->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v,
            focused->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].v);
        }
      }

      if (focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].c) {
        if (focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].c) {
          focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v = std::max(
            focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v,
            focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].v);
        }
        if (focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].c) {
          focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v = std::min(
            focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v,
            focused->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].v);
        }
      }
    }

    if (input_event->mouse_event.type == QB_MOUSE_EVENT_BUTTON) {
      qbMouseEvent e = &input_event->mouse_event;
      if (e->button.button == QB_BUTTON_LEFT && e->button.state == QB_MOUSE_UP) {
        focused = nullptr;
      }

      if (focused && focused->callbacks.onclick) {
        qbMouseEvent e = &input_event->mouse_event;
        event_handled |= focused->callbacks.onclick(focused, &e->button) == QB_TRUE;
      }
    } else if (input_event->mouse_event.type == QB_MOUSE_EVENT_SCROLL) {
      qbMouseEvent e = &input_event->mouse_event;
      if (closest && closest->callbacks.onscroll) {
        event_handled |= closest->callbacks.onscroll(closest, &e->scroll) == QB_TRUE;
      }
    }

    if (focused && focused->callbacks.onmove) {
      static int start_x = 0, start_y = 0;

      qbMouseEvent e = &input_event->mouse_event;
      if (e->button.button == QB_BUTTON_LEFT && e->button.state == QB_MOUSE_DOWN) {
        qb_mouse_getposition(&start_x, &start_y);
      }

      if (qb_mouse_ispressed(QB_BUTTON_LEFT)) {
        qbMouseMotionEvent_ e = {};

        qb_mouse_getposition(&e.x, &e.y);
        qb_mouse_getrelposition(&e.xrel, &e.yrel);
        if (e.xrel != 0 || e.yrel != 0) {
          event_handled |= focused->callbacks.onmove(focused, &e, start_x, start_y) == QB_TRUE;
        }
      }
    }
  } else if (input_event->type == QB_INPUT_EVENT_KEY) {
    if (focused && focused->callbacks.onkey) {
      focused->callbacks.onkey(focused, &input_event->key_event);
    }
  }

  return event_handled;
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
    attr.vs = get_gui_vs();
    attr.fs = get_gui_fs();
    attr.interpret_as_strings = true;

    attr.resources = resources;
    attr.resources_count = sizeof(resources) / sizeof(resources[0]);

    qb_shadermodule_create(&shader_module, &attr);
  }
  {    
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
    attr.name = "Gui Render Pass";
    attr.supported_geometry.bindings = &binding;
    attr.supported_geometry.bindings_count = 1;
    attr.supported_geometry.attributes = attributes;
    attr.supported_geometry.attributes_count = 3;
    attr.supported_geometry.mode = QB_DRAW_MODE_TRIANGLES;
    attr.shader = shader_module;
    attr.viewport = { 0.0, 0.0, (float)width, (float)height };
    attr.viewport_scale = 1.0f;
    attr.cull = QB_CULL_BACK;

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

    qb_gpubuffer_create(&default_vbo, &attr);
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
    qb_gpubuffer_create(&default_ebo, &attr);
  }
  {
    qbRenderGroupAttr_ attr = {};
    qb_rendergroup_create(&gui_render_group, &attr);
    qb_renderpass_append(gui_render_pass, gui_render_group);
  }

  return gui_render_pass;
}

void qb_gui_resize(uint32_t width, uint32_t height) {
  qb_renderpass_resize(gui_render_pass, { 0, 0, (float)width, (float)height });
  {
    GuiUniformCamera camera;
    camera.projection = glms_ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
    qb_gpubuffer_update(camera_ubo, 0, sizeof(GuiUniformCamera), &camera.projection);
  }
}

void qb_guielement_create_(qbGuiElement* el_ref, vec2s pos, vec2s size, bool open,
                         qbGuiElementCallbacks callbacks, qbGuiElement parent, qbImage background,
                         vec4s color, qbGpuBuffer ubo, qbMeshBuffer dbo, GuiRenderMode render_mode,
                         const char* name, qbVar state) {
  qbGuiElement el = *el_ref = new qbGuiElement_{};
  el->id = el_id++;
  el->state = state;
  el->parent = parent;
  el->rel_pos = pos;// glm::vec3{ pos.x, pos.y, 0.0f };
  el->pos = el->parent
    ? glms_vec2_add(el->parent->pos, el->rel_pos)
    : el->rel_pos;
  el->size = size;
  el->dirty = true;
  el->background = background;
  el->color = color;
  el->render_mode = render_mode;
  if (callbacks) {
    el->callbacks = *callbacks;
  } else {
    el->callbacks = {};
  }
  el->ubo = ubo;
  el->dbo = dbo;
  el->text_dbo = nullptr;
  el->name = std::string(name);
  el->text = nullptr;
  scene->Insert(el);

  if (open) {
    qb_guielement_open(el);
  }
}

void qb_guielement_create(qbGuiElement* el, const char* id, qbGuiElementAttr attr) {
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
    buffer_attr.descriptor = *qb_renderpass_geometry(gui_render_pass);

    qb_meshbuffer_create(&dbo, &buffer_attr);

    qbGpuBuffer vertex_buffers[] = { default_vbo };
    qb_meshbuffer_attachvertices(dbo, vertex_buffers, 4);
    qb_meshbuffer_attachindices(dbo, default_ebo, 6);

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
  qb_guielement_create_(el, vec2s{}, vec2s{}, true, attr->callbacks, nullptr, attr->background, attr->background_color, ubo, dbo, render_mode, id, attr->state);
  (*el)->text_scale = { 1.0, 1.0 };
  (*el)->radius = attr->radius;
  (*el)->offset = attr->offset;
  (*el)->visible = true;
  (*el)->text_color = attr->text_color;
  (*el)->text_align = attr->text_align;
  (*el)->text_size = attr->text_size;
  (*el)->text_count = 0;
  if (attr->font_name) {
    (*el)->font_name = attr->font_name;
  } else {
    (*el)->font_name = kDefaultFont;
  }
  (*el)->text = new utf8_t[1];
  (*el)->text[0] = '\0';
}

void qb_guielement_destroy(qbGuiElement* el) {
  scene->Destroy(*el);
  *el = nullptr;
}

void qb_guielement_destroy_(qbGuiElement el) {
  qbGpuBuffer* vertices;
  size_t vertices_count = qb_meshbuffer_vertices(el->dbo, &vertices);
  for (size_t i = 0; i < vertices_count; ++i) {
    if (vertices[i] == default_vbo) {
      continue;
    }
    qb_gpubuffer_destroy(&vertices[i]);
  }  

  qbGpuBuffer indices;
  qb_meshbuffer_indices(el->dbo, &indices);
  if (indices != default_ebo) {
    qb_gpubuffer_destroy(&indices);
  }
  qb_meshbuffer_destroy(&el->dbo);
  qb_gpubuffer_destroy(&el->ubo);

  if (el->text) {
    delete[] el->text;
  }

  delete el;
}

void qb_guielement_setconstraint(qbGuiElement el, qbGuiProperty property, qbConstraint constraint, float val) {
  el->constraints_[property][constraint] = { constraint, val };
}

float qb_guielement_getconstraint(qbGuiElement el, qbGuiProperty property, qbConstraint constraint) {
  return el->constraints_[property][constraint].v;
}

void qb_guielement_clearconstraint(qbGuiElement el, qbGuiProperty property, qbConstraint constraint) {
  el->constraints_[property][constraint] = { QB_CONSTRAINT_NONE, 0.f };
}

void qb_textbox_createtext(qbTextAlign align, size_t font_size, vec2s size, vec2s scale, const utf8_t* text, std::vector<float>* vertices, std::vector<int>* indices, qbImage* atlas) {
  Font* font = font_registry->Get(kDefaultFont, (uint32_t)font_size);
  FontRender renderer(font);
  renderer.Render(text, align, size, scale, vertices, indices);
  *atlas = font->Atlas();
}

void qb_guielement_link(qbGuiElement parent, qbGuiElement child) {
  scene->Move(child, parent);
  child->parent = parent;  
}

void qb_guielement_unlink(qbGuiElement child) {
  scene->Move(child, nullptr);
  child->parent = nullptr;  
}

QB_API const utf8_t* qb_guielement_gettext(qbGuiElement el) {
  return el->text;
}

// Todo: improve with decomposed signed distance fields: 
// https://gamedev.stackexchange.com/questions/150704/freetype-create-signed-distance-field-based-font
void qb_guielement_settext(qbGuiElement el, const utf8_t* text) {
  std::vector<float> vertices;
  std::vector<int> indices;
  qbImage font_atlas;
  qb_textbox_createtext(el->text_align, el->text_size, el->size, el->text_scale,
                        text, &vertices, &indices, &font_atlas);
  
  bool clearing_text = false;
  if (text != el->text) {
    if (text[0] == '\0' && el->text[0] != '\0') {
      clearing_text = true;
    }

    if (el->text) {
      delete[] el->text;
      el->text_count = 0;
    }
    el->text_count = text ? strlen(text) : 0;
    if (el->text_count == 0) {
      el->text = new utf8_t[1];
      memset(el->text, '\0', sizeof(utf8_t));
    } else {
      el->text = new utf8_t[el->text_count];
      memcpy(el->text, text, sizeof(utf8_t) * el->text_count);
    }
  } else {
    return;
  }

  if (el->text[0] == '\0' && !clearing_text) {
    return;
  }

  if (el->text_dbo) {
    qbGpuBuffer ebo, vbo, ubo;
    qbGpuBuffer* tmp;

    qb_meshbuffer_vertices(el->text_dbo, &tmp);
    vbo = tmp[0];
    qb_meshbuffer_indices(el->text_dbo, &ebo);
    qb_meshbuffer_uniforms(el->text_dbo, &tmp);
    ubo = tmp[0];

    qb_gpubuffer_destroy(&vbo);
    qb_gpubuffer_destroy(&ebo);
    qb_gpubuffer_destroy(&ubo);
    qb_meshbuffer_destroy(&el->text_dbo);
    el->text_dbo = nullptr;
  }

  qbGpuBuffer ubo, ebo, vbo;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(GuiUniformModel);
    qb_gpubuffer_create(&ubo, &attr);

    el->text_ubo = ubo;
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
    attr.descriptor = *qb_renderpass_geometry(gui_render_pass);

    qb_meshbuffer_create(&el->text_dbo, &attr);

    qbGpuBuffer vertex_buffers[] = { vbo };
    qb_meshbuffer_attachvertices(el->text_dbo, vertex_buffers, 4);
    qb_meshbuffer_attachindices(el->text_dbo, ebo, 6);

    uint32_t bindings[] = { GuiUniformModel::Binding() };
    qbGpuBuffer uniform_buffers[] = { ubo };
    qb_meshbuffer_attachuniforms(el->text_dbo, 1, bindings, uniform_buffers);

    uint32_t image_bindings[] = { 2 };
    qbImage images[] = { font_atlas };
    qb_meshbuffer_attachimages(el->text_dbo, 1, image_bindings, images);
  }

  el->dirty = true;
}

void qb_guielement_settextcolor(qbGuiElement el, vec4s text_color) {
  el->text_color = text_color;
  el->dirty = true;
}

void qb_guielement_settextscale(qbGuiElement el, vec2s scale) {
  el->text_scale = scale;
  el->dirty = true;
}

void qb_guielement_settextsize(qbGuiElement el, uint32_t font_size) {
  el->text_size = font_size;
  el->dirty = true;
}

void qb_guielement_settextalign(qbGuiElement el, qbTextAlign align) {
  el->text_align = align;
  el->dirty = true;
}

qbGuiElement qb_guielement_find(const char* name) {
  if (!name) {
    return nullptr;
  }

  if (name[0] == '\0') {
    return nullptr;
  }

  GuiNode* found = scene->Find(name);
  if (found) {
    return found->el;
  }
  return nullptr;
}

void qb_guielement_open(qbGuiElement el) {
  scene->Open(el);
}

void qb_guielement_close(qbGuiElement el) {
  scene->Close(el);
}

void qb_guielement_closechildren(qbGuiElement el) {
  scene->CloseChildren(el);
}

void qb_guielement_updateuniform(qbGuiElement el) {
  qb_guielement_updateuniform(el, el->render_mode, el->ubo);
  if (el->text_dbo) {
    qb_guielement_updateuniform(el, GUI_RENDER_MODE_STRING, el->text_ubo);
  }
}

void qb_guielement_updateuniform(qbGuiElement el, GuiRenderMode render_mode, qbGpuBuffer ubo) {
  mat4s model = GLMS_MAT4_IDENTITY_INIT;
  vec2s pos = glms_vec2_add(el->pos, { el->offset.x, el->offset.y });
  model = glms_translate(model, vec3s{ pos.x, pos.y, 0.f });

  if (render_mode == GUI_RENDER_MODE_STRING) {
    model = glms_scale(model, vec3s{ el->text_scale.x, el->text_scale.y, 1.0f });
  } else {
    model = glms_scale(model, vec3s{ el->size.x, el->size.y, 1.0f });
  }

  GuiUniformModel uniform = {};
  uniform.modelview = model;
  if (render_mode == GUI_RENDER_MODE_STRING) {
    uniform.color = el->text_color;
  } else {
    uniform.color = el->color;
  }
  uniform.render_mode = render_mode;
  uniform.radius = el->radius;
  uniform.size = el->size;  
  qb_gpubuffer_update(ubo, 0, sizeof(GuiUniformModel), &uniform);
}

void guielement_solve_constraints(qbGuiElement parent, qbGuiElement el) {
  if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_ASPECT_RATIO].c ||
      el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_ASPECT_RATIO].c) {

    if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_ASPECT_RATIO].c) {
      el->size.x =
          el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_ASPECT_RATIO].v * el->size.y;
    }

    if (el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_ASPECT_RATIO].c) {
      el->size.y =
          el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_ASPECT_RATIO].v * el->size.x;
    }
  } else {
    if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PIXEL].c) {
      el->size.x = el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PIXEL].v;
    } else if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PERCENT].c) {
      if (parent) {
        el->size.x = el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PERCENT].v * parent->size.x;
      } else {
        el->size.x = el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PERCENT].v * (float)qb_window_width();
      }
    } else if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_RELATIVE].c) {
      if (parent) {
        el->size.x = el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_RELATIVE].v + parent->size.x;
      } else {
        el->size.x = el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_RELATIVE].v + (float)qb_window_width();
      }
    }

    if (el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PIXEL].c) {
      el->size.y = el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PIXEL].v;
    } else if (el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PERCENT].c) {
      if (parent) {
        el->size.y = el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PERCENT].v * parent->size.y;
      } else {
        el->size.y = el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PERCENT].v * (float)qb_window_height();
      }
    } else if (el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_RELATIVE].c) {
      if (parent) {
        el->size.y = el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_RELATIVE].v + parent->size.y;
      } else {
        el->size.y = el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_RELATIVE].v + (float)qb_window_height();
      }
    }
  }

  if (el->constraints_[QB_GUI_X][QB_CONSTRAINT_PIXEL].c) {
    el->pos.x = el->constraints_[QB_GUI_X][QB_CONSTRAINT_PIXEL].v;
  } else if (el->constraints_[QB_GUI_X][QB_CONSTRAINT_PERCENT].c) {
    if (parent) {
      el->pos.x = el->constraints_[QB_GUI_X][QB_CONSTRAINT_PERCENT].v * parent->pos.x;
    } else {
      el->pos.x = el->constraints_[QB_GUI_X][QB_CONSTRAINT_PERCENT].v * qb_window_width();
    }

    if (el->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].c) {
      el->pos.x += el->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v;
    }
  } else if (el->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].c) {
    if (parent) {
      el->pos.x = el->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v + parent->pos.x;
    } else {
      el->pos.x = el->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v;
    }
  }

  if (el->constraints_[QB_GUI_Y][QB_CONSTRAINT_PIXEL].c) {
    el->pos.y = el->constraints_[QB_GUI_Y][QB_CONSTRAINT_PIXEL].v;
  } else if (el->constraints_[QB_GUI_Y][QB_CONSTRAINT_PERCENT].c) {
    if (parent) {
      el->pos.y = el->constraints_[QB_GUI_Y][QB_CONSTRAINT_PERCENT].v * parent->pos.y;
    } else {
      el->pos.y = el->constraints_[QB_GUI_Y][QB_CONSTRAINT_PERCENT].v * qb_window_height();
    }
    
    if (el->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].c) {
      el->pos.y += el->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v;
    }
  } else if (el->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].c) {
    if (parent) {
      el->pos.y = el->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v + parent->pos.y;
    } else {
      el->pos.y = el->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v;
    }
  }

  if (el->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].c) {
    if (parent) {
      el->pos.x = std::max(el->pos.x, parent->pos.x + el->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].v);
    } else {
      el->pos.x = std::max(el->pos.x, el->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].v);
    }
  }
  if (el->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].c) {
    if (parent) {
      el->pos.x = std::min(el->pos.x, parent->pos.x + el->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].v);
    } else {
      el->pos.x = std::min(el->pos.x, el->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].v);
    }
  }

  if (el->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].c) {
    if (parent) {
      el->pos.y = std::max(el->pos.y, parent->pos.y + el->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].v);
    } else {
      el->pos.y = std::max(el->pos.y, el->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].v);
    }
  }
  if (el->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].c) {
    if (parent) {
      el->pos.y = std::min(el->pos.y, parent->pos.y + el->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].v);
    } else {
      el->pos.y = std::min(el->pos.y, el->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].v);
    }
  }

  if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MIN].c) {
    el->size.x = std::max(el->size.x, el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MIN].v);
  }
  if (el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MAX].c) {
    el->size.x = std::min(el->size.x, el->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MAX].v);
  }

  if (el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MIN].c) {
    el->size.y = std::max(el->size.y, el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MIN].v);
  }
  if (el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MAX].c) {
    el->size.y = std::min(el->size.y, el->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MAX].v);
  }
}

void gui_element_updateuniforms() {
  scene->UpdateRenderGroup();

  scene->ForEach([](qbGuiElement el) {
    vec2s old_size = el->size;
    guielement_solve_constraints(el->parent, el);
    vec2s new_size = el->size;

    if (el->visible) {
      if (el->callbacks.onrender) {
        el->callbacks.onrender(el);
      }

      if ((el->dirty || !glms_vec2_eqv(old_size, new_size)) && el->text) {
        qb_guielement_settext(el, el->text);
      }

      qb_guielement_updateuniform(el);
      el->dirty = false;
    }    
  });
}

void qb_guielement_movetofront(qbGuiElement el) {
  scene->MoveToFront(el);
}

void qb_guielement_movetoback(qbGuiElement el) {
  scene->MoveToBack(el);
}

void qb_guielement_moveforward(qbGuiElement el) {
  scene->MoveForward(el);
}

void qb_guielement_movebackward(qbGuiElement el) {
  scene->MoveBackward(el);
}

void qb_guielement_moveto(qbGuiElement el, vec2s pos) {
  el->pos = pos;
  el->dirty = true;
}

void qb_guielement_resizeto(qbGuiElement el, vec2s size) {
  el->size = size;
  el->dirty = true;
}

void qb_guielement_moveby(qbGuiElement el, vec2s pos_delta) {
  el->pos = glms_vec2_add(el->pos, pos_delta);
  el->dirty = true;
}

void qb_guielement_resizeby(qbGuiElement el, vec2s size_delta) {
  el->size = glms_vec2_add(el->size, size_delta);
  el->dirty = true;
}

void qb_guielement_setvalue(qbGuiElement el, qbVar val) {
  if (el->callbacks.onsetvalue) {
    qbVar new_value = el->callbacks.onsetvalue(el, el->value, val);
    if (el->callbacks.onvaluechange) {
      el->callbacks.onvaluechange(el, el->value, new_value);      
    }
    el->value = new_value;
  }
  el->dirty = true;
}

qbVar qb_guielement_getvalue(qbGuiElement el) {
  if (el->callbacks.ongetvalue) {
    return el->callbacks.ongetvalue(el, el->value);
  }
  return el->value;
}

qbGuiElement qb_guielement_getfocus() {
  return focused;
}

qbGuiElement qb_guielement_getfocusat(float x, float y) {
  return scene->FindClosestBlock(x, y);
}

qbVar qb_guielement_getstate(qbGuiElement el) {
  return el->state;
}

vec2s qb_guielement_getsize(qbGuiElement el) {
  return el->size;
}

vec2s qb_guielement_getpos(qbGuiElement el) {
  return { el->pos.x, el->pos.y };
}

vec2s qb_guielement_getrelpos(qbGuiElement el) {
  return { el->rel_pos.x, el->rel_pos.y };
}

qbGuiElement qb_guielement_parent(qbGuiElement el) {
  return el->parent;
}