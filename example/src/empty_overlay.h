#ifndef EMPTY_OVERLAY__H
#define EMPTY_OVERLAY__H

#include <Framework/Overlay.h>

namespace cubez
{

class EmptyOverlay : public framework::Overlay {
public:

  EmptyOverlay(ultralight::Ref<ultralight::Renderer> renderer, ultralight::GPUDriver* driver,
          int width, int height, int x, int y, float scale);

  virtual void Draw() override;
  virtual void FireKeyEvent(const ultralight::KeyEvent& evt) override;
  virtual void FireMouseEvent(const ultralight::MouseEvent& evt) override;
  virtual void FireScrollEvent(const ultralight::ScrollEvent& evt) override;
  virtual void Resize(int width, int height) override;
};

}

#endif  // EMPTY_OVERLAY__H