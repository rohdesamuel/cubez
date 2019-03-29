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
#include <cubez/cubez.h>
#include "input.h"
#include "render.h"
#include "render_module.h"

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

typedef struct qbWindow_* qbWindow;
typedef cubez::TextureOverlay* qbRenderTarget;
typedef struct qbRenderTargetAttr_* qbRenderTargetAttr;
typedef struct qbGuiCallbacks_* qbGuiCallbacks;

struct Context {
  ultralight::RefPtr<ultralight::Renderer> renderer;
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
qbRenderPass CreateGuiRenderPass(qbFrameBuffer frame, uint32_t width, uint32_t height);

void qb_window_create(qbWindow* window, glm::vec3 pos, glm::vec2 size, bool open);
void qb_window_open(qbWindow window);
void qb_window_close(qbWindow window);

void qb_window_movetofront(qbWindow window);
void qb_window_movetoback(qbWindow window);

void qb_window_moveforward(qbWindow window);
void qb_window_movebackward(qbWindow window);

void qb_window_moveto(qbWindow window, glm::vec3 pos);
void qb_window_resizeto(qbWindow window, glm::vec2 size);

void qb_window_moveby(qbWindow window, glm::vec3 pos_delta);
void qb_window_resizeby(qbWindow window, glm::vec2 size_delta);

qbResult qb_guicallbacks_create(qbGuiCallbacks* callbacks);
qbResult qb_guicallbacks_onfocus(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_onclick(qbGuiCallbacks callbacks, void (*fn)(qbRenderTarget, input::MouseEvent*));
qbResult qb_guicallbacks_onkey(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget, input::InputEvent*));
qbResult qb_guicallbacks_onrender(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_onopen(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_onclose(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));
qbResult qb_guicallbacks_ondestroy(qbGuiCallbacks callbacks, void(*fn)(qbRenderTarget));

struct qbGuiCallbacks_ {
  void(*onfocus)(qbRenderTarget target);
  void(*onclick)(qbRenderTarget target, input::MouseEvent* e);
  void(*onkey)(qbRenderTarget target, input::InputEvent* e);
  void(*onrender)(qbRenderTarget target);
  void(*onopen)(qbRenderTarget target);
  void(*onclose)(qbRenderTarget target);
  void(*ondestroy)(qbRenderTarget target);
};
}

#endif  // GUI__H