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
typedef struct qbWindow_* qbWindow;

struct Context {
  ultralight::RefPtr<ultralight::Renderer> renderer;
};

struct Settings {
  const char* asset_dir;
  int width;
  int height;
};

typedef struct {
  void(*onfocus)(qbWindow window);
  void(*onclick)(qbWindow window, input::MouseEvent* e);
  void(*onscroll)(qbWindow window, input::MouseEvent* e);
  void(*onkey)(qbWindow window, input::InputEvent* e);
  void(*onmove)(qbWindow window);
  void(*onrender)(qbWindow window);
  void(*onopen)(qbWindow window);
  void(*onclose)(qbWindow window);
  void(*ondestroy)(qbWindow window);
} qbWindowCallbacks_, *qbWindowCallbacks;

typedef enum {
  QB_WINDOW_POSITION_ABSOLUTE,
  QB_WINDOW_POSITION_RELATIVE
} qbWindowPosition;

typedef struct {
  qbWindow parent;
  glm::vec3 pos;
  glm::vec2 size;
  bool open;

  glm::vec4 color;
  qbImage background_opt;
  qbWindowCallbacks callbacks_opt;
} qbWindowAttr_, *qbWindowAttr;

void Initialize();
void Shutdown();
void Render();
void HandleInput(SDL_Event* e);
qbRenderPass CreateGuiRenderPass(qbFrameBuffer frame, uint32_t width, uint32_t height);

void qb_window_create(qbWindow* window, glm::vec3 pos, glm::vec2 size, bool open,
                      qbWindowCallbacks callbacks, qbWindow parent, qbImage background,
                      glm::vec4 color);
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

void qb_window_updateuniforms();

}

#endif  // GUI__H