#ifndef GUI__H
#define GUI__H

#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <Ultralight/Renderer.h>
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>
#include <Framework/JSHelpers.h>
#include "cubez_gpu_driver.h"
#include <cubez/cubez.h>
#include "input.h"
#include "render.h"

#undef max
#undef min

namespace framework
{
class Overlay;
}

namespace cubez
{
class TextureOverlay;
}

namespace gui {

typedef std::function<framework::JSValue(const framework::JSObject&, const framework::JSArgs&)> JSCallback;
typedef std::unordered_map<std::string, JSCallback> JSCallbackMap;
typedef framework::Overlay* qbWindow;
typedef cubez::TextureOverlay* qbRenderTarget;
typedef struct qbRenderTargetAttr_* qbRenderTargetAttr;
typedef struct qbGuiCallbacks_* qbGuiCallbacks;
typedef JSValueRef(*qbJSCallback)(const JSObjectRef context, const JSValueRef* args, uint8_t num_args);
typedef struct qbJSCallbacks_* qbJSCallbacks;

struct Context {
  ultralight::RefPtr<ultralight::Renderer> renderer;
  std::unique_ptr<cubez::CubezGpuDriver> driver;
};

struct Settings {
  const char* asset_dir;
  int width;
  int height;
};

void Initialize(SDL_Window* win, Settings settings);
void Shutdown();
void Render();
void HandleInput(SDL_Event* e);

qbWindow FromFile(const std::string& file, glm::vec2 pos, glm::vec2 size, JSCallbackMap callback_map);
qbWindow FromHtml(const std::string& html, glm::vec2 pos, glm::vec2 size, JSCallbackMap callback_map);

void qb_window_fromfile(qbWindow* window, glm::vec2 pos, glm::vec2 size, qbGuiCallbacks callbacks);
void qb_window_fromhtml();

qbResult qb_jscallbacks_create(qbJSCallbacks* callbacks);
qbResult qb_jscallbacks_add(qbJSCallbacks callbacks, const char* fn, qbJSCallback callback);

qbResult qb_guicallbacks_create(qbGuiCallbacks* callbacks);
qbResult qb_guicallbacks_onfocus(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_onclick(qbGuiCallbacks callbacks, void (*fn)(qbRenderTarget, input::MouseEvent*));
qbResult qb_guicallbacks_onkey(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget, input::InputEvent*));
qbResult qb_guicallbacks_onrender(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_onopen(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_onclose(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_ondestroy(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));

qbResult qb_rendertarget_create(qbRenderTarget* target, glm::vec2 pos, glm::vec2 size, qbGuiCallbacks callbacks);
qbResult qb_rendertarget_bind(qbRenderTarget target);
qbResult qb_rendertarget_moveby(qbRenderTarget target, glm::vec2 delta);
qbResult qb_rendertarget_moveto(qbRenderTarget target, glm::vec2 pos);
qbResult qb_rendertarget_resize(qbRenderTarget target, glm::vec2 size);

void CloseWindow(qbWindow window);


}

#endif  // GUI__H