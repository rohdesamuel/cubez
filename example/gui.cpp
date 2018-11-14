#include "inc/common.h"
#include "cubez_gpu_driver.h"
#include "empty_overlay.h"

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
#include <Framework/platform/linux/FileSystemLinux.h>
#include <Framework/platform/linux/FontLoaderLinux.h>
#endif
#include <Framework/Platform.h>

#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include "gui.h"

namespace gui {

SDL_Window* win;
ultralight::RefPtr<ultralight::Renderer> renderer;
std::unique_ptr<cubez::CubezGpuDriver> driver;
std::unique_ptr<cubez::EmptyOverlay> empty_overlay;

void Initialize(SDL_Window* sdl_window, Settings settings) {
  win = sdl_window;

  ultralight::Platform& platform = ultralight::Platform::instance();

  ultralight::Config config;
  config.face_winding = ultralight::FaceWinding::kFaceWinding_CounterClockwise;
  config.device_scale_hint = 1.0;
  config.use_distance_field_fonts = config.device_scale_hint != 1.0f; // Only use SDF fonts for high-DPI
  config.use_distance_field_paths = true;

  driver = std::make_unique<cubez::CubezGpuDriver>(settings.width, settings.height, 1.0);

  platform.set_config(config);
  platform.set_gpu_driver(driver.get());
  platform.set_font_loader(framework::CreatePlatformFontLoader());
  platform.set_file_system(framework::CreatePlatformFileSystem(settings.asset_dir));

  renderer = ultralight::Renderer::Create();

  empty_overlay = std::make_unique<cubez::EmptyOverlay>(*renderer.get(), driver.get(), 500, 500, 0, 0, 1.0f);
}

void Shutdown() {
}

void Render() {
  renderer->Update();
  driver->BeginSynchronize();
  renderer->Render();
  driver->EndSynchronize();

  if (driver->HasCommandsPending()) {    
    driver->DrawCommandList();
  }
  empty_overlay->Draw();
}

SDL_Window* Window() {
  return win;
}

void HandleInput(SDL_Event*) {

}

}