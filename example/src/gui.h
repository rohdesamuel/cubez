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

#undef max
#undef min

namespace framework
{
class Overlay;
}

namespace gui {

typedef std::function<framework::JSValue(const framework::JSObject&, const framework::JSArgs&)> JSCallback;
typedef std::unordered_map<std::string, JSCallback> JSCallbackMap;
typedef framework::Overlay* Window;

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

Window FromFile(const std::string& file, glm::vec2 pos, glm::vec2 size, JSCallbackMap callback_map);
Window FromHtml(const std::string& html, glm::vec2 pos, glm::vec2 size, JSCallbackMap callback_map);

void CloseWindow(Window window);

}

#endif  // GUI__H