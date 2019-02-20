#include "overlay.h"
#include "cubez/cubez.h"

namespace cubez
{

using framework::JSArgs;
using framework::JSObject;
using framework::JSValue;

JSValue Unimplemented(const JSObject&, const JSArgs&) {
  assert(false && "Unimplemented");
  return JSValue();
}


std::unique_ptr<Overlay> Overlay::FromHtml(
  const std::string& html, const gui::Context& context, Properties&& properties,
  gui::JSCallbackMap callbacks) {

  Overlay* overlay = new Overlay(context, std::move(properties), std::move(callbacks));
  overlay->view()->set_load_listener(overlay);
  overlay->view()->LoadHTML(html.c_str());

  return std::unique_ptr<Overlay>(overlay);
}

std::unique_ptr<Overlay> Overlay::FromFile(
  const std::string& file, const gui::Context& context, Properties&& properties,
  gui::JSCallbackMap callbacks) {

  Overlay* overlay = new Overlay(context, std::move(properties), std::move(callbacks));
  overlay->view()->set_load_listener(overlay);
  overlay->view()->LoadURL(file.c_str());
  
  return std::unique_ptr<Overlay>(overlay);
}

Overlay::Overlay(const gui::Context& context, Properties&& properties, gui::JSCallbackMap callbacks) :
  framework::Overlay(*context.renderer, context.driver.get(),
                     (int)properties.size.x, (int)properties.size.y,
                     (int)properties.pos.x, (int)properties.pos.y, properties.scale, true),
  callbacks_(callbacks) {
}

void Overlay::OnDOMReady(ultralight::View* caller) {
  framework::SetJSContext(caller->js_context());
  framework::JSObject global = framework::JSGlobalObject();

  for (auto callback_pair : callbacks_) {
    global[callback_pair.first.c_str()] = callback_pair.second;
  }
}

void Overlay::Draw() {
  framework::Overlay::Draw();
}

void Overlay::FireKeyEvent(const ultralight::KeyEvent& evt) {
  framework::Overlay::FireKeyEvent(evt);
}

void Overlay::FireMouseEvent(const ultralight::MouseEvent& evt) {
  framework::Overlay::FireMouseEvent(evt);
}

void Overlay::FireScrollEvent(const ultralight::ScrollEvent& evt) {
  framework::Overlay::FireScrollEvent(evt);
}

void Overlay::Resize(int width, int height) {
  framework::Overlay::Resize(width, height);
}

}