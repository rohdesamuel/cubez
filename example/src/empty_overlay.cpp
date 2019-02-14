#include "empty_overlay.h"

namespace cubez
{

EmptyOverlay::EmptyOverlay(ultralight::Ref<ultralight::Renderer> renderer, ultralight::GPUDriver* driver,
                           int width, int height, int x, int y, float scale) :
  Overlay(renderer, driver, width, height, x, y, scale) {
  view()->LoadHTML(R"(
    <html>
      <head>
        <title>Hello World</title>
      </head>
      <body style="background-color:red;opacity:0.5">
        <div style="background-color:blue;width:100px;height:100px;opacity:0.5;">
        div
        </div>
        text
      </body>
    </html>)");
}

void EmptyOverlay::Draw() {
  Overlay::Draw();
}

void EmptyOverlay::FireKeyEvent(const ultralight::KeyEvent& evt) {
  Overlay::FireKeyEvent(evt);
}

void EmptyOverlay::FireMouseEvent(const ultralight::MouseEvent& evt) {
  Overlay::FireMouseEvent(evt);
}

void EmptyOverlay::FireScrollEvent(const ultralight::ScrollEvent& evt) {
  Overlay::FireScrollEvent(evt);
}

void EmptyOverlay::Resize(int width, int height) {
  Overlay::Resize(width, height);
}

}
