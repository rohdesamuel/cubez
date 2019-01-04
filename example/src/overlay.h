#ifndef OVERLAY__H
#define OVERLAY__H

#include <glm/glm.hpp>
#include <Framework/Overlay.h>
#include <Framework/JSHelpers.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "gui.h"

namespace cubez
{

class Overlay : public framework::Overlay,
                public ultralight::LoadListener {
public:
  struct Properties {
    glm::vec2 size;
    glm::vec2 pos;
    float scale;
  };

  static std::unique_ptr<Overlay> FromHtml(const std::string& html,
                                           const gui::Context& context,
                                           Properties&& properties,
                                           gui::JSCallbackMap callbacks);
  static std::unique_ptr<Overlay> FromFile(const std::string& file,
                                           const gui::Context& context,
                                           Properties&& properties,
                                           gui::JSCallbackMap callbacks);

  virtual void OnDOMReady(ultralight::View* caller) override;

  virtual void Draw() override;
  virtual void FireKeyEvent(const ultralight::KeyEvent& evt) override;
  virtual void FireMouseEvent(const ultralight::MouseEvent& evt) override;
  virtual void FireScrollEvent(const ultralight::ScrollEvent& evt) override;
  virtual void Resize(int width, int height) override;

private:
  Overlay(const gui::Context& context, Properties&& properties, gui::JSCallbackMap callbacks);

  gui::JSCallbackMap callbacks_;
};

}

#endif  // OVERLAY__H