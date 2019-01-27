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

namespace gui
{

Context context;
SDL_Window* win;
std::vector<std::unique_ptr<framework::Overlay>> overlays;
std::vector<std::unique_ptr<framework::Overlay>> closed_overlays;

enum qbWindowType {
  QB_WINDOW_TYPE_OVERLAY = 0,
  QB_WINODW_TYPE_RENDER_TARGET
};

struct qbWindow_ {
  qbWindowType type;
  union {
    framework::Overlay* overlay;
    qbRenderTarget render_target;
  };
};

void Initialize(SDL_Window* sdl_window, Settings settings) {
  win = sdl_window;

  ultralight::Platform& platform = ultralight::Platform::instance();

  ultralight::Config config;
  config.face_winding = ultralight::FaceWinding::kFaceWinding_CounterClockwise;
  config.device_scale_hint = 1.0;
  config.use_distance_field_fonts = config.device_scale_hint != 1.0f; // Only use SDF fonts for high-DPI
  config.use_distance_field_paths = true;
  config.enable_images = true;

  context.driver = std::make_unique<cubez::CubezGpuDriver>(settings.width, settings.height, 1.0);

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
  for (auto& overlay : overlays) {
    overlay->Draw();
  }
}

void HandleInput(SDL_Event* e) {
  ultralight::KeyEvent key_event;
  ultralight::MouseEvent mouse_event;
  
  if (e->type == SDL_MOUSEBUTTONDOWN) {
    mouse_event.type = ultralight::MouseEvent::kType_MouseDown;
    mouse_event.button = ultralight::MouseEvent::Button::kButton_Left;
    mouse_event.x = e->button.x;
    mouse_event.y = e->button.y;
  } else if (e->type == SDL_MOUSEBUTTONUP) {
    mouse_event.type = ultralight::MouseEvent::kType_MouseUp;
    mouse_event.button = ultralight::MouseEvent::Button::kButton_Left;
    mouse_event.x = e->button.x;
    mouse_event.y = e->button.y;
  } else if (e->type == SDL_MOUSEMOTION) {
    mouse_event.type = ultralight::MouseEvent::kType_MouseMoved;
    mouse_event.x = e->motion.x;
    mouse_event.y = e->motion.y;
    mouse_event.button = ultralight::MouseEvent::Button::kButton_Left;
  }
  for (auto& overlay : overlays) {
    overlay->FireMouseEvent(mouse_event);
  }
}

qbWindow FromFile(const std::string& file, glm::vec2 pos, glm::vec2 size, JSCallbackMap callback_map) {
  cubez::Overlay::Properties properties;
  properties.size = size;
  properties.pos = pos;
  properties.scale = 1.0f;

  auto overlay = cubez::Overlay::FromFile(file, context, std::move(properties), std::move(callback_map));

  overlays.push_back(std::move(overlay));
  return overlays.back().get();
}

qbWindow FromHtml(const std::string& html, glm::vec2 pos, glm::vec2 size, JSCallbackMap callback_map) {
  cubez::Overlay::Properties properties;
  properties.size = size;
  properties.pos = pos;
  properties.scale = 1.0f;

  auto overlay = cubez::Overlay::FromHtml(html, context, std::move(properties), std::move(callback_map));

  overlays.push_back(std::move(overlay));
  return overlays.back().get();
}

void CloseWindow(qbWindow window) {
  size_t i = 0;
  for (auto& overlay : overlays) {
    if (overlay.get() == window) {
      overlays.erase(overlays.begin() + i);
      return;
    }
    ++i;
  }
}

struct qbJsCallbacks_ {

};

struct qbGuiCallbacks_ {
  void(*onfocus)(qbRenderTarget target);
  void(*onclick)(qbRenderTarget target, input::MouseEvent* e);
  void(*onkey)(qbRenderTarget target, input::InputEvent* e);
  void(*onrender)(qbRenderTarget target);
  void(*onopen)(qbRenderTarget target);
  void(*onclose)(qbRenderTarget target);
  void(*ondestroy)(qbRenderTarget target);
};

struct qbRenderTarget_ {
  qbGuiCallbacks_ callbacks;
  cubez::TextureOverlay* overlay;
};

struct qbRenderTargetAttr_ {

};

qbResult qb_guicallbacks_create(qbGuiCallbacks* callbacks) {
  *callbacks = new qbGuiCallbacks_();
  memset(*callbacks, 0, sizeof(qbGuiCallbacks_));
  return QB_OK;
}
qbResult qb_guicallbacks_destroy(qbGuiCallbacks* callbacks) {
  delete *callbacks;
  *callbacks = nullptr;
  return QB_OK;
}

qbResult qb_guicallbacks_onfocus(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget)) {
  callbacks->onfocus = fn;
  return QB_OK;
}

qbResult qb_guicallbacks_onclick(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget, input::MouseEvent*)) {
  callbacks->onclick = fn;
  return QB_OK;
}

qbResult qb_guicallbacks_onkey(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget, input::InputEvent*)) {
  callbacks->onkey = fn;
  return QB_OK;
}

qbResult qb_guicallbacks_onrender(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget)) {
  callbacks->onrender = fn;
  return QB_OK;
}

qbResult qb_guicallbacks_onopen(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget)) {
  callbacks->onopen = fn;
  return QB_OK;
}

qbResult qb_guicallbacks_onclose(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget)) {
  callbacks->onclose = fn;
  return QB_OK;
}

qbResult qb_guicallbacks_ondestroy(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget)) {
  callbacks->ondestroy = fn;
  return QB_OK;
}

qbResult qb_rendertarget_create(qbRenderTarget* render_target, glm::vec2 pos, glm::vec2 size,
                                qbGuiCallbacks callbacks) {
  cubez::TextureOverlay::Properties properties;
  properties.size = size;
  properties.pos = pos;
  properties.scale = 1.0f;

  auto overlay = std::make_unique<cubez::TextureOverlay>(context, std::move(properties));
  overlays.push_back(std::move(overlay));

  *render_target = (cubez::TextureOverlay*)overlays.back().get();

  return QB_OK;
}

qbResult qb_rendertarget_bind(qbRenderTarget render_target) {
  render_target->overlay->Bind();
  return QB_OK;
}

qbResult qb_rendertarget_moveby(qbRenderTarget render_target, glm::vec2 delta) {
  render_target->overlay->MoveBy(delta.x, delta.y);
  return QB_OK;
}

qbResult qb_rendertarget_moveto(qbRenderTarget render_target, glm::vec2 pos) {
  render_target->overlay->MoveTo(pos.x, pos.y);
  return QB_OK;
}

qbResult qb_rendertarget_resize(qbRenderTarget render_target, glm::vec2 size) {
  render_target->overlay->Resize(size.x, size.y);
  return QB_OK;
}

}
