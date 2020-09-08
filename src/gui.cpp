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

struct qbGuiBlock_ {
  std::string name;
  uint32_t id;
  qbGuiBlock parent;

  // X, Y are block coordinates.
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

  char16_t* text;
  size_t text_count;

  // True if there is a need to update the buffers.
  bool dirty;
  bool visible;

  vec4s color;
  vec4s text_color;
  vec2s scale;
  uint32_t font_size;
  qbTextAlign align;
  qbImage background;
  float radius;
  vec2s offset;

  qbConstraint_ constraints_[(int)QB_GUI_HEIGHT + 1][(int)QB_CONSTRAINT_ASPECT_RATIO + 1];
  qbVar value;
  qbGuiBlockCallbacks_ callbacks;
};

qbRenderPass gui_render_pass;
qbRenderGroup gui_render_group;
qbGpuBuffer block_vbo;
qbGpuBuffer block_ebo;
qbGpuBuffer camera_ubo;

std::atomic_uint block_id;
qbGuiBlock focused;

FontRegistry* font_registry;
const char* kDefaultFont = "opensans";
const int kMaxPathLength = 10;

void qb_guiblock_updateuniform(qbGuiBlock block);

struct GuiNode {
  GuiNode(qbGuiBlock block) : block(block) {}

  qbGuiBlock block;
  std::vector<std::pair<qbGuiBlockCallbacks_, qbVar>> callbacks;
  std::unordered_map<std::string, std::unique_ptr<struct GuiNode>> children;
  std::vector<struct GuiNode*> ordered_children;
};

class GuiTree {
public:
  GuiTree() : scene_(nullptr) {}
  void Insert(qbGuiBlock block) {
    GuiNode* parent_node = Insert(block->parent, block);

    if (parent_node->children.find(block->name) == parent_node->children.end()) {
      GuiNode* block_node = new GuiNode(block);

      parent_node->children[block->name] = std::unique_ptr<GuiNode>(block_node);
      auto found = std::find(parent_node->ordered_children.begin(), parent_node->ordered_children.end(),
                             block_node);
      if (found != parent_node->ordered_children.end()) {
        parent_node->ordered_children.push_back(block_node);
      } else {
        parent_node->ordered_children.push_back(block_node);
      }
    }
  }

  void Erase(qbGuiBlock block) {
    GuiNode* parent_node = Erase(block->parent, block);

    if (parent_node->children.find(block->name) != parent_node->children.end()) {
      auto& found_block_node = parent_node->children.find(block->name);
      GuiNode* block_node = found_block_node->second.get();
      parent_node->children.erase(found_block_node);

      auto found = std::find(parent_node->ordered_children.begin(), parent_node->ordered_children.end(), block_node);
      if (found != parent_node->ordered_children.end()) {
        parent_node->ordered_children.erase(found);
      }
    }
  }

  void Destroy(qbGuiBlock block) {
    GuiNode* parent = FindParent(block->parent, block)->children[block->name].get();
    GuiNode* node = parent->children.find(block->name)->second.get();

    // Record what blocks to destroy.
    std::vector<qbGuiBlock> to_destroy;
    ForEach([this, &to_destroy](qbGuiBlock b) {
      to_destroy.push_back(b);
    }, node);

    // Remove from tree.
    Erase(block);

    // Call the destroy callback in reverse topological order.
    for (auto& it = to_destroy.rbegin(); it != to_destroy.rend(); ++it) {
      if ((*it)->callbacks.ondestroy) {
        (*it)->callbacks.ondestroy(*it);        
      }
    }

    // Only after all children have had a chance to perform their callbacks, actually free the block.
    for (auto& block : to_destroy) {
      qb_guiblock_destroy_(block);
    }
  }

  void Open(qbGuiBlock block) {
    if (!block->visible && block->callbacks.onopen) {
      block->callbacks.onopen(block);
    }
    block->visible = true;
  }

  void Close(qbGuiBlock block) {
    if (block->visible && block->callbacks.onclose) {
      block->callbacks.onclose(block);
    }
    block->visible = false;
  }

  void CloseChildren(qbGuiBlock block) {
    GuiNode* node = FindParent(block->parent, block)->children[block->name].get();
    ForEach([this, block](qbGuiBlock b) {
      if (b == block) return;
      Close(b);
    }, node);
  }

  void Move(qbGuiBlock block, qbGuiBlock new_parent) {
    auto old_parent_node = FindParent(block);
    auto found = FindParent(new_parent);
    auto new_parent_node = found->children[new_parent->name].get();

    if (old_parent_node == new_parent_node) {
      return;
    }

    auto& node = old_parent_node->children.find(block->name);
    GuiNode* block_node = node->second.get();
    new_parent_node->children[node->first] = std::move(node->second);
    new_parent_node->ordered_children.push_back(block_node);

    old_parent_node->children.erase(block->name);
    old_parent_node->ordered_children.erase(std::find(
      old_parent_node->ordered_children.begin(),
      old_parent_node->ordered_children.end(),
      block_node
    ));
  }

  qbGuiBlock FindClosestBlock(int x, int y) {
    qbGuiBlock found = nullptr;
    ForEachBreakoutEarly([x, y, &found](qbGuiBlock b) {
      if (x >= b->pos.x + b->offset.x &&
          x < b->pos.x + b->offset.x + b->size.x &&
          y >= b->pos.y + b->offset.y  &&
          y < b->pos.y + b->offset.y + b->size.y) {
        found = b;
        return true;
      }
      return false;
    }, &scene_);

    return found;
  }

  qbGuiBlock FindClosestBlock(int x, int y, std::function<bool(qbGuiBlock)> handler) {
    qbGuiBlock found = nullptr;
    ForEachBreakoutEarly([x, y, &found, &handler](qbGuiBlock b) {
      if (x >= b->pos.x + b->offset.x &&
          x < b->pos.x + b->offset.x + b->size.x &&
          y >= b->pos.y + b->offset.y  &&
          y < b->pos.y + b->offset.y + b->size.y) {
        found = b;
        if (handler(found)) {
          return true;
        }
      }
      return false;
    }, &scene_);

    return found;
  }

  qbGuiBlock FindFarthestBlock(int x, int y) {
    qbGuiBlock found;
    ForEach([x, y, &found](qbGuiBlock b) {
      if (x >= b->pos.x + b->offset.x &&
          x < b->pos.x + b->offset.x + b->size.x &&
          y >= b->pos.y + b->offset.y  &&
          y < b->pos.y + b->offset.y + b->size.y) {
        found = b;
      }
    }, &scene_);

    return found;
  }

  void MoveToFront(qbGuiBlock block) {
    auto parent_node = FindParent(block);
    GuiNode* block_node = parent_node->children[block->name].get();

    auto& it = std::find(parent_node->ordered_children.begin(), parent_node->ordered_children.end(), block_node);
    parent_node->ordered_children.erase(it);
    parent_node->ordered_children.push_back(block_node);
  }

  void MoveToBack(qbGuiBlock block) {
    auto parent_node = FindParent(block);
    GuiNode* block_node = parent_node->children[block->name].get();

    auto& it = std::find(parent_node->ordered_children.begin(), parent_node->ordered_children.end(), block_node);
    parent_node->ordered_children.erase(it);
    parent_node->ordered_children.insert(parent_node->ordered_children.begin(), block_node);
  }

  void MoveForward(qbGuiBlock block) {
    auto parent_node = FindParent(block);
    GuiNode* block_node = parent_node->children[block->name].get();

    if (parent_node->ordered_children.size() > 1) {
      auto& it = std::find(parent_node->ordered_children.begin(), parent_node->ordered_children.end(), block_node);
      if (it == parent_node->ordered_children.end() - 1) {
        return;
      }

      std::swap(*it, *(it + 1));
    }
  }

  void MoveBackward(qbGuiBlock block) {
    auto parent_node = FindParent(block);
    GuiNode* block_node = parent_node->children[block->name].get();

    if (parent_node->ordered_children.size() > 1) {
      auto& it = std::find(parent_node->ordered_children.begin(), parent_node->ordered_children.end(), block_node);
      if (it == parent_node->ordered_children.begin()) {
        return;
      }

      std::swap(*it, *(it - 1));
    }
  }

  GuiNode* FindParent(qbGuiBlock block) {
    return FindParent(block->parent, block);
  }

  GuiNode* Find(const char* path[]) {
    if (!path) {
      return{};
    }

    GuiNode* cur = &scene_;
    GuiNode* end_of_path = nullptr;
    size_t i;
    for (i = 0; i < kMaxPathLength; ++i) {
      if (!path[i] || (path[i] && path[i][0] == '*')) {
        if (i > 0) {
          end_of_path = cur;
        }
        break;
      }

      auto found = cur->children.find(std::string(path[i]));
      if (found == cur->children.end()) {
        break;
      }

      cur = found->second.get();
    }

    if (i == kMaxPathLength) {
      FATAL("Path exceeded max length of 10.");
    }

    return end_of_path;
  }

  std::vector<GuiNode*> FindAll(const char* path[]) {
    GuiNode* end_of_path = Find(path);

    if (!end_of_path) {
      return{};
    }

    bool glob = false;
    for (int i = 0; i < kMaxPathLength; ++i) {
      if (!path[i]) {
        break;
      }

      if (path[i][0] == '*' && path[i][1] == '\0') {
        glob = true;
      }
    }

    std::vector<GuiNode*> ret{end_of_path};
    if (glob) {
      for (auto& c : end_of_path->children) {
        FindAll(&ret, c.second.get());
      }
    }
    return ret;
  }

  template<class F>
  void ForEach(const F& f) {
    ForEach(f, &scene_);
  }

  void UpdateRenderGroup() {
    std::vector<qbMeshBuffer> buffers;
    buffers.reserve(25);

    UpdateRenderGroup(&buffers, &scene_);

    qb_rendergroup_update(gui_render_group, buffers.size(), buffers.data());
  }

private:
  void UpdateRenderGroup(std::vector<qbMeshBuffer>* buffers, GuiNode* node) {
    if (node == &scene_ || node->block->visible) {
      if (node->block) {
        buffers->push_back(node->block->dbo);
      }
      for (auto& c : node->ordered_children) {
        UpdateRenderGroup(buffers, c);
      }
    }    
  }

  template<class F>
  void ForEach(const F& f, GuiNode* node) {
    if (node->block) {
      f(node->block);
    }

    for (auto it = node->ordered_children.rbegin(); it != node->ordered_children.rend(); ++it) {
      ForEach(f, *it);
    }
  }

  template<class F>
  bool ForEachBreakoutEarly(const F& f, GuiNode* node) {
    
    for (auto it = node->ordered_children.rbegin(); it != node->ordered_children.rend(); ++it) {
      auto& n = *it;
      if (ForEachBreakoutEarly(f, n)) {
        return true;
      }
    }

    if (node->block) {
      if (f(node->block)) {
        return true;
      }
    }

    return false;
  }

  void FindAll(std::vector<GuiNode*>* ret, GuiNode* node) {
    ret->push_back(node);
    for (auto& c : node->children) {
      FindAll(ret, c.second.get());
    }
  }

  GuiNode* FindParent(qbGuiBlock parent, qbGuiBlock block) {
    // At the root.
    if (!parent) {
      return &scene_;
    }

    GuiNode* parent_node = FindParent(parent->parent, block);
    auto found_parent = parent_node->children.find(parent->name);
    if (found_parent != parent_node->children.end()) {
      return found_parent->second.get();
    }

    return parent_node;
  }

  GuiNode* Insert(qbGuiBlock parent, qbGuiBlock block) {
    // At the root.
    if (!parent) {
      return &scene_;
    }

    GuiNode* parent_node = Insert(parent->parent, block);
    auto found_parent = parent_node->children.find(parent->name);
    if (found_parent != parent_node->children.end()) {
      return found_parent->second.get();
    }

    parent_node->children[parent->name] = std::make_unique<GuiNode>(parent);
    return parent_node->children[parent->name].get();
  }

  GuiNode* Erase(qbGuiBlock parent, qbGuiBlock block) {
    // At the root.
    if (!parent) {
      return &scene_;
    }

    GuiNode* parent_node = Erase(parent->parent, block);
    auto found_parent = parent_node->children.find(parent->name);
    if (found_parent != parent_node->children.end()) {
      return found_parent->second.get();
    } else {
      parent_node->children[parent->name] = std::make_unique<GuiNode>(parent);
      return parent_node->children[parent->name].get();
    }

    if (parent_node->children.find(block->name) != parent_node->children.end()) {
      parent_node->children.erase(block->name);
    }
  }

  GuiNode scene_;
};
std::unique_ptr<GuiTree> root;

void gui_initialize() {
  font_registry = new FontRegistry();
  font_registry->Load("resources/fonts/OpenSans-Bold.ttf", "opensans");
  root = std::make_unique<GuiTree>();
}

void gui_shutdown() {
}

bool gui_handle_input(qbInputEvent input_event) {
  int x, y;
  qb_mouse_position(&x, &y);

  bool event_handled = false;
  qbGuiBlock old_focused = focused;
  qbGuiBlock closest = nullptr;

  if (input_event->type == qbInputEventType::QB_INPUT_EVENT_MOUSE) {
    closest = root->FindClosestBlock(x, y, [input_event](qbGuiBlock closest) {
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
        event_handled |= focused->callbacks.onfocus(focused);
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
    } else if (input_event->mouse_event.type == QB_MOUSE_EVENT_SCROLL) {
      qbMouseEvent e = &input_event->mouse_event;
      if (closest && closest->callbacks.onscroll) {
        event_handled |= closest->callbacks.onscroll(closest, &e->scroll);
      }
    }

    if (focused && focused->callbacks.onclick) {
      qbMouseEvent e = &input_event->mouse_event;
      event_handled |= focused->callbacks.onclick(focused, &e->button);
    }

    if (focused && focused->callbacks.onmove) {
      static int start_x = 0, start_y = 0;

      qbMouseEvent e = &input_event->mouse_event;
      if (e->button.button == QB_BUTTON_LEFT && e->button.state == QB_MOUSE_DOWN) {
        qb_mouse_position(&start_x, &start_y);
      }

      if (qb_mouse_pressed(QB_BUTTON_LEFT)) {
        qbMouseMotionEvent_ e = {};

        qb_mouse_position(&e.x, &e.y);
        qb_mouse_relposition(&e.xrel, &e.yrel);
        if (e.xrel != 0 || e.yrel != 0) {
          event_handled |= focused->callbacks.onmove(focused, &e, start_x, start_y);
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
    attr.vs = "resources/gui.vs";
    attr.fs = "resources/gui.fs";

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

    qb_gpubuffer_create(&block_vbo, &attr);
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
qb_gpubuffer_create(&block_ebo, &attr);
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

void qb_guiblock_create_(qbGuiBlock* block_ref, vec3s pos, vec2s size, bool open,
                         qbGuiBlockCallbacks callbacks, qbGuiBlock parent, qbImage background,
                         vec4s color, qbGpuBuffer ubo, qbMeshBuffer dbo, GuiRenderMode render_mode,
                         const char* name) {
  qbGuiBlock block = *block_ref = new qbGuiBlock_{};
  block->id = block_id++;
  block->parent = parent;
  block->rel_pos = pos;// glm::vec3{ pos.x, pos.y, 0.0f };
  block->pos = block->parent
    ? glms_vec3_add(block->parent->pos, block->rel_pos)
    : block->rel_pos;
  block->size = size;
  block->dirty = true;
  block->background = background;
  block->color = color;
  block->render_mode = render_mode;
  if (callbacks) {
    block->callbacks = *callbacks;
  } else {
    block->callbacks = {};
  }
  block->ubo = ubo;
  block->dbo = dbo;
  block->name = std::string(name);
  block->text = nullptr;
  root->Insert(block);

  if (open) {
    qb_guiblock_open(block);
  }
}

void qb_guiblock_create(qbGuiBlock* block, qbGuiBlockAttr attr, const char* name) {
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

    qbGpuBuffer vertex_buffers[] = { block_vbo };
    qb_meshbuffer_attachvertices(dbo, vertex_buffers);
    qb_meshbuffer_attachindices(dbo, block_ebo);

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
  qb_guiblock_create_(block, vec3s{}, vec2s{}, true, attr->callbacks, nullptr, attr->background, attr->background_color, ubo, dbo, render_mode, name);
  (*block)->scale = { 1.0, 1.0 };
  (*block)->radius = attr->radius;
  (*block)->offset = attr->offset;
  (*block)->visible = true;
}

void qb_guiblock_destroy(qbGuiBlock* block) {
  root->Destroy(*block);
  *block = nullptr;
}

void qb_guiblock_destroy_(qbGuiBlock block) {
  qbGpuBuffer* vertices;
  size_t vertices_count = qb_meshbuffer_vertices(block->dbo, &vertices);
  for (size_t i = 0; i < vertices_count; ++i) {
    if (vertices[i] == block_vbo) {
      continue;
    }
    qb_gpubuffer_destroy(&vertices[i]);
  }  

  qbGpuBuffer indices;
  qb_meshbuffer_indices(block->dbo, &indices);
  if (indices != block_ebo) {
    qb_gpubuffer_destroy(&indices);
  }
  qb_meshbuffer_destroy(&block->dbo);
  qb_gpubuffer_destroy(&block->ubo);

  if (block->text) {
    delete[] block->text;
  }

  delete block;
}

void qb_guiconstraint(qbGuiBlock block, qbConstraint constraint, qbGuiProperty property, float val) {
  block->constraints_[property][constraint] = { constraint, val };
}

float qb_guiconstraint_get(qbGuiBlock block, qbConstraint constraint, qbGuiProperty property) {
  return block->constraints_[property][constraint].v;
}

void qb_guiconstraint_x(qbGuiBlock block, qbConstraint constraint, float val) {
  block->constraints_[QB_GUI_X][constraint] = { constraint, val };
  block->dirty = true;
}

void qb_guiconstraint_y(qbGuiBlock block, qbConstraint constraint, float val) {
  block->constraints_[QB_GUI_Y][constraint] = { constraint, val };
  block->dirty = true;
}

void qb_guiconstraint_width(qbGuiBlock block, qbConstraint constraint, float val) {
  block->constraints_[QB_GUI_WIDTH][constraint] = { constraint, val };
  block->dirty = true;
}

void qb_guiconstraint_height(qbGuiBlock block, qbConstraint constraint, float val) {
  block->constraints_[QB_GUI_HEIGHT][constraint] = { constraint, val };
  block->dirty = true;
}

void qb_guiconstraint_clear(qbGuiBlock block, qbConstraint constraint, qbGuiProperty property) {
  block->constraints_[property][constraint] = { QB_CONSTRAINT_NONE, 0.f };
}

void qb_guiconstraint_clearx(qbGuiBlock block, qbConstraint constraint) {
  block->constraints_[QB_GUI_X][constraint] = { QB_CONSTRAINT_NONE, 0.f};
  block->dirty = true;
}

void qb_guiconstraint_cleary(qbGuiBlock block, qbConstraint constraint) {
  block->constraints_[QB_GUI_Y][constraint] = { QB_CONSTRAINT_NONE, 0.f };
  block->dirty = true;
}

void qb_guiconstraint_clearwidth(qbGuiBlock block, qbConstraint constraint) {
  block->constraints_[QB_GUI_WIDTH][constraint] = { QB_CONSTRAINT_NONE, 0.f };
  block->dirty = true;
}

void qb_guiconstraint_clearheight(qbGuiBlock block, qbConstraint constraint) {
  block->constraints_[QB_GUI_HEIGHT][constraint] = { QB_CONSTRAINT_NONE, 0.f };
  block->dirty = true;
}

void qb_textbox_createtext(qbTextAlign align, size_t font_size, vec2s size, vec2s scale, const char16_t* text, std::vector<float>* vertices, std::vector<int>* indices, qbImage* atlas) {
  Font* font = font_registry->Get(kDefaultFont, (uint32_t)font_size);
  FontRender renderer(font);
  renderer.Render(text, align, size, scale, vertices, indices);
  *atlas = font->Atlas();
}

void qb_guiblock_link(qbGuiBlock parent, qbGuiBlock child) {
  root->Move(child, parent);
  child->parent = parent;  
}

void qb_guiblock_unlink(qbGuiBlock child) {
  root->Move(child, nullptr);
  child->parent = nullptr;  
}

int strlen16(const char16_t* strarg) {
  if (!strarg)
    return -1; //strarg is NULL pointer
  const char16_t* str = strarg;
  for (; *str; ++str)
    ; // empty body
  return str - strarg;
}

// Todo: improve with decomposed signed distance fields: 
// https://gamedev.stackexchange.com/questions/150704/freetype-create-signed-distance-field-based-font
void qb_textbox_create(qbGuiBlock* block,
                       qbTextboxAttr textbox_attr,
                       const char* name,
                       vec2s size,
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
    attr.descriptor = *qb_renderpass_geometry(gui_render_pass);

    qb_meshbuffer_create(&dbo, &attr);

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

  qb_guiblock_create_(block, vec3s{}, size,
                      true, nullptr,
                      nullptr, nullptr, textbox_attr->text_color,
                      ubo, dbo, GUI_RENDER_MODE_STRING, name);
  (*block)->scale = { 1.0f, 1.0f };
  (*block)->text_color = textbox_attr->text_color;
  (*block)->align = textbox_attr->align;
  (*block)->font_size = font_size;
  if ((*block)->text_count == 0) {
    (*block)->text = new char16_t[1];
    memset((*block)->text, L'\0', sizeof(char16_t));
  } else {
    (*block)->text = new char16_t[(*block)->text_count];
    memcpy((*block)->text, text, sizeof(char16_t) * (*block)->text_count);
  }
}

void qb_textbox_text(qbGuiBlock block, const char16_t* text) {
  std::vector<float> vertices;
  std::vector<int> indices;
  qbImage font_atlas;
  qb_textbox_createtext(block->align, block->font_size, block->size, block->scale,
                        text, &vertices, &indices, &font_atlas);
  
  if (text != block->text) {
    if (block->text) {
      delete[] block->text;
      block->text_count = 0;
    }
    block->text_count = text ? strlen16(text) : 0;
    if (block->text_count == 0) {
      block->text = new char16_t[1];
      memset(block->text, L'\0', sizeof(char16_t));
    } else {
      block->text = new char16_t[block->text_count];
      memcpy(block->text, text, sizeof(char16_t) * block->text_count);
    }
  }

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
  qb_meshbuffer_vertices(block->dbo, &dst_vbos);
  qbGpuBuffer old = dst_vbos[0];
  dst_vbos[0] = vbo;
  qb_meshbuffer_attachvertices(block->dbo, dst_vbos);
  qb_gpubuffer_destroy(&old);
  
  qbGpuBuffer dst_ebo;
  qb_meshbuffer_indices(block->dbo, &dst_ebo);
  qb_meshbuffer_attachindices(block->dbo, ebo);
  qb_gpubuffer_destroy(&dst_ebo);
}

void qb_textbox_color(qbGuiBlock block, vec4s text_color) {
  block->text_color = text_color;
}

void qb_textbox_scale(qbGuiBlock block, vec2s scale) {
  block->scale = scale;
}

void qb_textbox_fontsize(qbGuiBlock block, uint32_t font_size) {
  block->font_size = font_size;
}

qbGuiBlock qb_guiblock_find(const char* path[]) {
  GuiNode* found = root->Find(path);
  if (found) {
    return found->block;
  }
  return nullptr;
}

void qb_guiblock_open(qbGuiBlock block) {
  root->Open(block);
}

void qb_guiblock_openquery(const char* path[]) {
  auto found = root->FindAll(path);
  for (auto node : found) {
    qb_guiblock_open(node->block);
  }
}

void qb_guiblock_close(qbGuiBlock block) {
  root->Close(block);
}

void qb_guiblock_closequery(const char* path[]) {
  auto found = root->FindAll(path);
  for (auto node : found) {
    qb_guiblock_close(node->block);
  }
}

void qb_guiblock_closechildren(qbGuiBlock block) {
  root->CloseChildren(block);
}

void qb_guiblock_updateuniform(qbGuiBlock block) {
  mat4s model = GLMS_MAT4_IDENTITY_INIT;
  model = glms_translate(model, glms_vec3_add(block->pos, { block->offset.x, block->offset.y, 0.f }));

  if (block->render_mode == GUI_RENDER_MODE_STRING) {
    model = glms_scale(model, vec3s{ block->scale.x, block->scale.y, 1.0f });
  } else {
    model = glms_scale(model, vec3s{ block->size.x, block->size.y, 1.0f });
  }

  block->uniform.modelview = model;
  block->uniform.color = block->color;
  block->uniform.render_mode = block->render_mode;
  block->uniform.size = block->size;
  block->uniform.radius = block->radius;
  qb_gpubuffer_update(block->ubo, 0, sizeof(GuiUniformModel), &block->uniform);
}

void guiblock_solve_constraints(qbGuiBlock parent, qbGuiBlock block) {
  if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_ASPECT_RATIO].c ||
      block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_ASPECT_RATIO].c) {

    if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_ASPECT_RATIO].c) {
      block->size.x =
          block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_ASPECT_RATIO].v * block->size.y;
    }

    if (block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_ASPECT_RATIO].c) {
      block->size.y =
          block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_ASPECT_RATIO].v * block->size.x;
    }
  } else {
    if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PIXEL].c) {
      block->size.x = block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PIXEL].v;
    } else if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PERCENT].c) {
      if (parent) {
        block->size.x = block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PERCENT].v * parent->size.x;
      } else {
        block->size.x = block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_PERCENT].v * (float)qb_window_width();
      }
    } else if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_RELATIVE].c) {
      if (parent) {
        block->size.x = block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_RELATIVE].v + parent->size.x;
      } else {
        block->size.x = block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_RELATIVE].v + (float)qb_window_width();
      }
    }

    if (block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PIXEL].c) {
      block->size.y = block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PIXEL].v;
    } else if (block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PERCENT].c) {
      if (parent) {
        block->size.y = block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PERCENT].v * parent->size.y;
      } else {
        block->size.y = block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_PERCENT].v * (float)qb_window_height();
      }
    } else if (block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_RELATIVE].c) {
      if (parent) {
        block->size.y = block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_RELATIVE].v + parent->size.y;
      } else {
        block->size.y = block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_RELATIVE].v + (float)qb_window_height();
      }
    }
  }

  if (block->constraints_[QB_GUI_X][QB_CONSTRAINT_PIXEL].c) {
    block->pos.x = block->constraints_[QB_GUI_X][QB_CONSTRAINT_PIXEL].v;
  } else if (block->constraints_[QB_GUI_X][QB_CONSTRAINT_PERCENT].c) {
    if (parent) {
      block->pos.x = block->constraints_[QB_GUI_X][QB_CONSTRAINT_PERCENT].v * parent->pos.x;
    } else {
      block->pos.x = block->constraints_[QB_GUI_X][QB_CONSTRAINT_PERCENT].v * qb_window_width();
    }

    if (block->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].c) {
      block->pos.x += block->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v;
    }
  } else if (block->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].c) {
    if (parent) {
      block->pos.x = block->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v + parent->pos.x;
    } else {
      block->pos.x = block->constraints_[QB_GUI_X][QB_CONSTRAINT_RELATIVE].v;
    }
  }

  if (block->constraints_[QB_GUI_Y][QB_CONSTRAINT_PIXEL].c) {
    block->pos.y = block->constraints_[QB_GUI_Y][QB_CONSTRAINT_PIXEL].v;
  } else if (block->constraints_[QB_GUI_Y][QB_CONSTRAINT_PERCENT].c) {
    if (parent) {
      block->pos.y = block->constraints_[QB_GUI_Y][QB_CONSTRAINT_PERCENT].v * parent->pos.y;
    } else {
      block->pos.y = block->constraints_[QB_GUI_Y][QB_CONSTRAINT_PERCENT].v * qb_window_height();
    }
    
    if (block->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].c) {
      block->pos.y += block->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v;
    }
  } else if (block->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].c) {
    if (parent) {
      block->pos.y = block->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v + parent->pos.y;
    } else {
      block->pos.y = block->constraints_[QB_GUI_Y][QB_CONSTRAINT_RELATIVE].v;
    }
  }

  if (block->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].c) {
    if (parent) {
      block->pos.x = std::max(block->pos.x, parent->pos.x + block->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].v);
    } else {
      block->pos.x = std::max(block->pos.x, block->constraints_[QB_GUI_X][QB_CONSTRAINT_MIN].v);
    }
  }
  if (block->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].c) {
    if (parent) {
      block->pos.x = std::min(block->pos.x, parent->pos.x + block->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].v);
    } else {
      block->pos.x = std::min(block->pos.x, block->constraints_[QB_GUI_X][QB_CONSTRAINT_MAX].v);
    }
  }

  if (block->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].c) {
    if (parent) {
      block->pos.y = std::max(block->pos.y, parent->pos.y + block->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].v);
    } else {
      block->pos.y = std::max(block->pos.y, block->constraints_[QB_GUI_Y][QB_CONSTRAINT_MIN].v);
    }
  }
  if (block->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].c) {
    if (parent) {
      block->pos.y = std::min(block->pos.y, parent->pos.y + block->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].v);
    } else {
      block->pos.y = std::min(block->pos.y, block->constraints_[QB_GUI_Y][QB_CONSTRAINT_MAX].v);
    }
  }

  if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MIN].c) {
    block->size.x = std::max(block->size.x, block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MIN].v);
  }
  if (block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MAX].c) {
    block->size.x = std::min(block->size.x, block->constraints_[QB_GUI_WIDTH][QB_CONSTRAINT_MAX].v);
  }

  if (block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MIN].c) {
    block->size.y = std::max(block->size.y, block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MIN].v);
  }
  if (block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MAX].c) {
    block->size.y = std::min(block->size.y, block->constraints_[QB_GUI_HEIGHT][QB_CONSTRAINT_MAX].v);
  }
}

void gui_block_updateuniforms() {
  root->UpdateRenderGroup();

  root->ForEach([](qbGuiBlock block) {
    vec2s old_size = block->size;
    guiblock_solve_constraints(block->parent, block);
    vec2s new_size = block->size;

    if (block->visible) {
      if (block->callbacks.onrender) {
        block->callbacks.onrender(block);
      }
      qb_guiblock_updateuniform(block);
      block->dirty = false;
    }

    if (!glms_vec2_eqv(old_size, new_size) && block->text) {
      qb_textbox_text(block, block->text);
    }    
  });
}

void qb_guiblock_movetofront(qbGuiBlock block) {
  root->MoveToFront(block);
}

void qb_guiblock_movetoback(qbGuiBlock block) {
  root->MoveToBack(block);
}

void qb_guiblock_moveforward(qbGuiBlock block) {
  root->MoveForward(block);
}

void qb_guiblock_movebackward(qbGuiBlock block) {
  root->MoveBackward(block);
}

void qb_guiblock_moveto(qbGuiBlock block, vec3s pos) {
  block->pos = pos;
  block->dirty = true;
}

void qb_guiblock_resizeto(qbGuiBlock block, vec2s size) {
  block->size = size;
  block->dirty = true;
}

void qb_guiblock_moveby(qbGuiBlock block, vec3s pos_delta) {
  block->pos = glms_vec3_add(block->pos, pos_delta);
  block->dirty = true;
}

void qb_guiblock_resizeby(qbGuiBlock block, vec2s size_delta) {
  block->size = glms_vec2_add(block->size, size_delta);
  block->dirty = true;
}

void qb_guiblock_setvalue(qbGuiBlock block, qbVar val) {
  if (block->callbacks.setvalue) {
    qbVar new_value = block->callbacks.setvalue(block, block->value, val);
    if (block->callbacks.onvaluechange) {
      block->callbacks.onvaluechange(block, block->value, new_value);      
    }
    block->value = new_value;
  }
}

qbVar qb_guiblock_value(qbGuiBlock block) {
  if (block->callbacks.getvalue) {
    return block->callbacks.getvalue(block, block->value);
  }
  return block->value;
}

qbGuiBlock qb_guiblock_focus() {
  return focused;
}

qbGuiBlock qb_guiblock_focusat(int x, int y) {
  return root->FindClosestBlock(x, y);
}

vec2s qb_guiblock_size(qbGuiBlock block) {
  return block->size;
}

vec2s qb_guiblock_pos(qbGuiBlock block) {
  return { block->pos.x, block->pos.y };
}

vec2s qb_guiblock_relpos(qbGuiBlock block) {
  return { block->rel_pos.x, block->rel_pos.y };
}

qbGuiBlock qb_guiblock_parent(qbGuiBlock block) {
  return block->parent;
}

void qb_gui_subscribe(const char* path[], qbGuiBlockCallbacks callbacks, qbVar user_state) {
  GuiNode* node = root->Find(path);
  if (!node) {
    return;
  }

  node->callbacks.push_back({ *callbacks, user_state });
}