#include <cubez/cubez.h>
#include "cubez_gpu_driver.h"
#include "overlay.h"

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
#include "texture_overlay.h"

#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include "gui.h"
#include "empty_overlay.h"
#include <atomic>
#include <iostream>

namespace gui
{

// std140 layout
struct GuiUniformCamera {
  alignas(16) glm::mat4 projection;
  
  static uint32_t Binding() {
    return 0;
  }
};

// std140 layout
struct GuiUniformModel {
  alignas(16) glm::mat4 modelview;

  static uint32_t Binding() {
    return 1;
  }
};

struct qbWindow_ {
  uint32_t id;

  // X, Y are window coordinates.
  // Z is depth
  glm::vec3 pos;
  glm::vec2 size;

  GuiUniformModel uniform;
  qbGpuBuffer ubo;
  qbDrawBuffer dbo;
};

Context context;
SDL_Window* win;
qbRenderPass gui_render_pass;
qbGpuBuffer vbo;
qbGpuBuffer ebo;

std::atomic_uint window_id;
std::vector<qbDrawBuffer> window_buffers;
std::vector<std::unique_ptr<qbWindow_>> windows;
typename decltype(windows) closed_windows;

void qb_window_updateuniforms(qbWindow window);

namespace
{

decltype(windows)::iterator find_window(decltype(windows)& container, qbWindow window) {
  return std::find_if(
    container.begin(), container.end(),
    [window](const auto& win) {
      return win.get() == window;
    });
}

}

void Initialize(SDL_Window* sdl_window, Settings settings) {
  win = sdl_window;
  window_id = 0;

  ultralight::Platform& platform = ultralight::Platform::instance();

  ultralight::Config config;
  config.face_winding = ultralight::FaceWinding::kFaceWinding_CounterClockwise;
  config.device_scale_hint = 1.0;
  config.use_distance_field_fonts = config.device_scale_hint != 1.0f; // Only use SDF fonts for high-DPI
  config.use_distance_field_paths = true;
  config.enable_images = true;

  context.driver = std::make_unique<cubez::CubezGpuDriver>(settings.width, settings.height, (GLfloat)1.0f);

  platform.set_config(config);
  platform.set_gpu_driver(context.driver.get());
  platform.set_font_loader(framework::CreatePlatformFontLoader());
  platform.set_file_system(framework::CreatePlatformFileSystem(settings.asset_dir));

  context.renderer = ultralight::Renderer::Create();
}

void Shutdown() {
}

void Render() {
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  context.renderer->Update();
  context.driver->BeginSynchronize();
  context.renderer->Render();
  context.driver->EndSynchronize();

  if (context.driver->HasCommandsPending()) {
    context.driver->DrawCommandList();
  }
}

void HandleInput(SDL_Event* e) {
  if (e->type == SDL_MOUSEBUTTONDOWN) {
    glm::vec2 pos{ e->button.x, e->button.y };
    qbWindow max_depth = nullptr;
    for (int32_t i = 0; i < windows.size(); ++i) {
      qbWindow window = windows[i].get();
      if (pos.x > window->pos.x &&
          pos.x < window->pos.x + window->size.x &&
          pos.y > window->pos.y &&
          pos.y < window->pos.y + window->size.y) {
        max_depth = window;
        break;
      }
    }
    if (max_depth) {
      qb_window_movetofront(max_depth);
      std::cout << "Clicked on " << max_depth->id << std::endl;
    }

  } else if (e->type == SDL_MOUSEBUTTONUP) {
  } else if (e->type == SDL_MOUSEMOTION) {
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
    qbShaderResourceAttr_ resource_attrs[2];
    {
      qbShaderResourceAttr attr = resource_attrs;
      attr->binding = GuiUniformCamera::Binding();
      attr->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
      attr->stages = QB_SHADER_STAGE_VERTEX;
      attr->name = "Camera";
    }
    {
      qbShaderResourceAttr attr = resource_attrs + 1;
      attr->binding = GuiUniformModel::Binding();
      attr->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
      attr->stages = QB_SHADER_STAGE_VERTEX;
      attr->name = "Model";
    }
    /*{
      qbShaderResourceAttr attr = resource_attrs + 2;
      attr->binding = 2;
      attr->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      attr->stages = QB_SHADER_STAGE_VERTEX;
      attr->name = "tex_sampler";
    }*/

    qbShaderModuleAttr_ attr;
    attr.vs = "resources/gui.vs";
    attr.fs = "resources/gui.fs";

    attr.resources = resource_attrs;
    attr.resources_count = sizeof(resource_attrs) / sizeof(resource_attrs[0]);

    qb_shadermodule_create(&shader_module, &attr);
  }
  {
    qbGpuBuffer camera_ubo;
    {
      qbGpuBufferAttr_ attr;
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
      camera.projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
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
      qb_imagesampler_create(image_samplers, &attr);
    }

    uint32_t bindings[] = { 2 };
    qbImageSampler* samplers = image_samplers;
    //qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
  }

  {
    qbRenderPassAttr_ attr;
    attr.frame_buffer = frame;

    attr.bindings = &binding;
    attr.bindings_count = 1; // 1

    attr.attributes = attributes;
    attr.attributes_count = 3; // 2

    attr.shader_program = shader_module;

    attr.viewport = { 0.0, 0.0, width, height };
    attr.viewport_scale = 1.0f;

    qb_renderpass_create(&gui_render_pass, &attr, nullptr);
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

    qb_gpubuffer_create(&vbo, &attr);
  }
  {
    int indices[] = {
      0, 1, 3,
      3, 2, 1
    };

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = indices;
    attr.size = sizeof(indices);
    attr.elem_size = sizeof(indices[0]);
    qb_gpubuffer_create(&ebo, &attr);
  }

  return gui_render_pass;
}

void qb_window_create(qbWindow* window_ref, glm::vec3 pos, glm::vec2 size, bool open) {
  qbWindow window = *window_ref = new qbWindow_;
  window->id = window_id++;
  
  window->pos = glm::vec3{ pos.x, pos.y, -0.5f };
  window->size = size;

  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(GuiUniformModel);
    qb_gpubuffer_create(&window->ubo, &attr);

    qb_window_updateuniforms(window);
  }
  {
    qbDrawBufferAttr_ attr;
    qb_drawbuffer_create(&window->dbo, &attr, gui_render_pass);

    qbGpuBuffer vertex_buffers[] = { vbo };
    qb_drawbuffer_attachvertices(window->dbo, vertex_buffers);
    qb_drawbuffer_attachindices(window->dbo, ebo);

    uint32_t bindings[] = { GuiUniformModel::Binding() };
    qbGpuBuffer uniform_buffers[] = { window->ubo };
    qb_drawbuffer_attachuniforms(window->dbo, 1, bindings, uniform_buffers);
  }

  window_buffers.push_back(window->dbo);
  closed_windows.push_back(std::unique_ptr<qbWindow_>(window));

  if (open) {
    qb_window_open(window);
  }
}

void qb_window_open(qbWindow window) {
  auto found = find_window(closed_windows, window);
  if (found != closed_windows.end()) {
    windows.push_back(std::move(*found));
    closed_windows.erase(found);
  }
}

void qb_window_close(qbWindow window) {

}

void qb_window_updateuniforms(qbWindow window) {
  glm::mat4 model = glm::mat4(1.0);
  model = glm::translate(model, glm::vec3(window->pos));
  model = glm::translate(model, glm::vec3(0.5f * window->size.x, 0.5f * window->size.y, 0.0f));
  //model = glm::rotate(model, rotate, glm::vec3(0.0f, 0.0f, 1.0f));
  model = glm::translate(model, glm::vec3(-0.5f * window->size.x, -0.5f * window->size.y, 0.0f));
  model = glm::scale(model, glm::vec3(window->size, 1.0f));

  window->uniform.modelview = model;
  qb_gpubuffer_update(window->ubo, 0, sizeof(GuiUniformModel), &window->uniform.modelview);
}

void qb_window_movetofront(qbWindow window) {
  auto found = find_window(windows, window);
  if (found != windows.end() && windows.size() > 1) {
    uint32_t found_index = found - windows.begin();
    uint32_t swap_index = 0;
    std::swap(*found, *(windows.begin() + swap_index));
    std::swap(window_buffers[found_index], window_buffers[swap_index]);

    qb_renderpass_updatedrawbuffers(gui_render_pass, window_buffers.size(), window_buffers.data());
  }
}

void qb_window_movetoback(qbWindow window);

void qb_window_moveforward(qbWindow window);
void qb_window_movebackward(qbWindow window);

void qb_window_moveto(qbWindow window, glm::vec3 pos) {
  window->pos = pos;
  window->pos.z = glm::clamp(window->pos.z, -0.9999f, 0.9999f);
  qb_window_updateuniforms(window);
}

void qb_window_resizeto(qbWindow window, glm::vec2 size) {
  window->size = size;
  qb_window_updateuniforms(window);
}

void qb_window_moveby(qbWindow window, glm::vec3 pos_delta) {
  window->pos += pos_delta;
  window->pos.z = glm::clamp(window->pos.z, -0.9999f, 0.9999f);
  qb_window_updateuniforms(window);
}

void qb_window_resizeby(qbWindow window, glm::vec2 size_delta) {
  window->size += size_delta;
  qb_window_updateuniforms(window);
}

}
