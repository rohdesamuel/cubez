#ifndef GUI__H
#define GUI__H

#include <glm/glm.hpp>
#include <string>

namespace framework
{
class Overlay;
}

namespace gui {

typedef framework::Overlay* Window;

struct Settings {
  const char* asset_dir;
  int width;
  int height;
};

void Initialize(SDL_Window* win, Settings settings);

void Shutdown();

void Render();

void HandleInput(SDL_Event* e);

Window OpenWindow(const std::string& file, glm::vec2 pos, glm::vec2 size);
void CloseWindow(Window window);

}

#endif  // GUI__H