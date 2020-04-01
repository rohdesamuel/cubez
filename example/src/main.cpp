#include <cubez/cubez.h>
#include <cubez/utils.h>
#include <cubez/log.h>
#include <cubez/mesh.h>
#include <cubez/input.h>
#include <cubez/render.h>
#include <cubez/audio.h>
#include <cubez/render_pipeline.h>
#include <cubez/gui.h>
#include "forward_renderer.h"

#include "pong.h"

#include <algorithm>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <glm/ext.hpp>

qbVar print_timing_info(qbVar var) {
  for (;;) {
    qbTiming info = (qbTiming)var.p;

    std::string out =
      "Frame: " + std::to_string(info->frame) + "\n"
      "Total FPS:  " + std::to_string(info->total_fps) + "\n"
      "Render FPS: " + std::to_string(info->render_fps) + "\n"
      "Update FPS: " + std::to_string(info->udpate_fps) + "\n";
    //qb_log(QB_INFO, out.c_str());
    qb_coro_wait(1.0);
  }
}

void main_menu_activate(qbScene scene, size_t count, const char* keys[], void* values[]) {
  auto it = std::find(keys, keys + count, "main_menu");
  if (it != keys + count) {
    return;
  }
}

qbTiming_ timing_info;
qbScene main_menu_scene;
qbScene game_scene;
void create_main_menu() {
  qb_scene_create(&main_menu_scene, "Main Menu");
  qb_scene_set(main_menu_scene);

  qbWindowCallbacks_ window_callbacks = {};
  window_callbacks.onfocus = [](qbWindow) {
    std::cout << "onfocus\n";
    return true;
  };
  window_callbacks.onclick = [](qbWindow, qbMouseButtonEvent e) {
    std::cout << "onclick ( " << e->button << ", " << (bool)e->state << " )\n";
    return true;
  };
  window_callbacks.onscroll = [](qbWindow window, qbMouseScrollEvent e) {
    std::cout << "onscroll ( " << e->xrel << ", " << e->yrel << " )\n";
    if (qb_is_key_pressed(qbKey::QB_KEY_LSHIFT)) {
      qb_window_moveby(window, { e->yrel * 5, 0.0f, 0.0f });
    } else {
      qb_window_moveby(window, { e->xrel, e->yrel * 5, 0.0f });
    }
    return true;
  };

  qbImage ball_image;
  {
    qbImageAttr_ attr;
    attr.type = QB_IMAGE_TYPE_2D;
    qb_image_load(&ball_image, &attr, "resources/soccer_ball.bmp");
  }

  std::vector<qbWindow> windows;
  qbWindow parent;

  {
    qbWindowAttr_ attr = {};
    attr.background_color = { 0.0, 0.0, 0.0, 0.95 };
    attr.callbacks = &window_callbacks;
    qb_window_create(&parent, &attr, { 0.0f, 0.0f }, { 512.0f, 64.0f }, nullptr, true);
  }

  qbWindow text_box;
  {
    qbTextboxAttr_ textbox_attr = {};
    textbox_attr.align = QB_TEXT_ALIGN_LEFT;
    textbox_attr.text_color = { 1.0f, 0.0f, 0.0f, 1.0f };
    qb_textbox_create(&text_box, &textbox_attr, { 16.0f, 0.0f }, { 512.0f, 64.0f }, parent, true, 48, u"");
  }
  qb_window_movetoback(parent);
  qb_window_movetofront(text_box);

  qbWindow main_menu;
  {
    qbWindowAttr_ attr = {};
    attr.background_color = { 0.0, 0.0, 0.0, 0.95 };

    qb_window_create(&main_menu, &attr, { (1200.0f * 0.5f) - 256.0f, 100.0f }, { 512.0f, 600.0f }, nullptr, true);
    windows.push_back(main_menu);
  }

  qbWindow main_menu_border;
  {
    qbWindowAttr_ attr = {};
    attr.background_color = { 1.0, 1.0, 1.0, 0.9 };

    qb_window_create(&main_menu_border, &attr, { -4.0f, -4.0f }, { 520.0f, 608.0f }, main_menu, true);
    qb_window_movetoback(main_menu_border);
    windows.push_back(main_menu_border);
  }

  qbWindow new_game;
  {
    qbWindowCallbacks_ callbacks = {};
    callbacks.onclick = [](qbWindow w, qbMouseButtonEvent e) {
      return true;
    };
    callbacks.onscroll = [](qbWindow w, qbMouseScrollEvent e) {
      qb_window_resizeby(w, { e->yrel, 0 });
      return true;
    };
    qbWindowAttr_ attr = {};
    attr.background_color = { 1.0, 1.0, 1.0, 0.15 };
    attr.callbacks = &callbacks;

    qb_window_create(&new_game, &attr, { 0.0f, 100.0f }, { 512.0f, 64.0f }, main_menu, true);
    windows.push_back(new_game);
  }

  {
    qbTextboxAttr_ attr = {};
    attr.align = QB_TEXT_ALIGN_CENTER;
    attr.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    qbWindow new_game_text;
    qb_textbox_create(&new_game_text, &attr, { 0.0f, 0.0f }, { 512.0f, 64.0f }, new_game, true, 48, u"New Game");
    windows.push_back(new_game_text);
  }
  
  qb_window_close(main_menu);
  //qb_window_open(main_menu);

  qb_scene_attach(main_menu_scene, "main_menu", new std::vector<qbWindow>(std::move(windows)));
  qb_coro_sync([](qbVar text) {
    qbWindow text_box = (qbWindow)text.p;
    while (true) {
      int fps = (int)timing_info.total_fps;
      {
        std::wstring text_fps = std::to_wstring(fps);
        while (text_fps.length() < 4) {
          text_fps.insert(text_fps.begin(), L'0');
        }
        text_fps = std::wstring(L"FPS: ") + text_fps;
        qb_textbox_text(text_box, (char16_t*)text_fps.data());
      }
      qb_coro_waitframes(10);
    }

    return qbNone;
  }, qbVoid(text_box));

  //qb_window_movetofront(text_box);
  qb_scene_reset();
}

void create_game() {
  qb_scene_create(&game_scene, "Game");
  qb_scene_set(game_scene);

  qbCamera main_camera;
  {
    qbCameraAttr_ attr = {};
    attr.fov = 3.14159f / 4.0f;
    attr.height = 800;
    attr.width = 1200;
    attr.znear = 0.1;
    attr.zfar = 1000;
    attr.origin = glm::vec3(-10, 0, 0);
    attr.rotation_mat = glm::mat4(1);
    qb_camera_create(&main_camera, &attr);
    qb_camera_activate(main_camera);
  }
  create_main_menu();

  {
    qb_light_point(0, { 1.0, 1.0, 1.0 }, { 0.0, 0.0, 5.0 }, 15.0f, 200.0f);
    qb_light_point(1, { 1.0, 0.0, 0.0 }, { -25.0, 0.0, 5.0 }, 10.0f, 200.0f);
    qb_light_point(2, { 0.0, 1.0, 0.0 }, { 25.0, 0.0, 5.0 }, 10.0f, 200.0f);
    qb_light_point(3, { 0.0, 0.0, 1.0 }, { 0.0, -25.0, 5.0 }, 10.0f, 200.0f);
    qb_light_point(4, { 1.0, 0.0, 1.0 }, { 0.0, 25.0, 5.0 }, 10.0f, 200.0f);
    qb_light_enable(0, qbLightType::QB_LIGHT_TYPE_POINT);
    qb_light_enable(1, qbLightType::QB_LIGHT_TYPE_POINT);
    qb_light_enable(2, qbLightType::QB_LIGHT_TYPE_POINT);
    qb_light_enable(3, qbLightType::QB_LIGHT_TYPE_POINT);
    qb_light_enable(4, qbLightType::QB_LIGHT_TYPE_POINT);

    qb_light_directional(0, { 0.9, 0.9, 1.0 }, { 0.0, 0.0, -1.0 }, 0.05f);
    qb_light_directional(1, { 0.9, 0.9, 1.0 }, { 0.0, 0.0, 1.0 }, 0.1f);
    qb_light_enable(0, qbLightType::QB_LIGHT_TYPE_DIRECTIONAL);
    //qb_light_enable(1, qbLightType::QB_LIGHT_TYPE_DIRECTIONAL);
  }

  qb_coro_sync(print_timing_info, qbVoid(&timing_info));
  qbEntity block;
  {
    qbMaterial material;
    {
      qbMaterialAttr_ attr = {};
      {
        qbImageAttr_ image_attr = {};
        image_attr.type = qbImageType::QB_IMAGE_TYPE_2D;
        image_attr.generate_mipmaps = true;
        qb_image_load(&attr.albedo_map, &image_attr,
                      "resources/soccer_ball.bmp");
        qb_image_load(&attr.normal_map, &image_attr,
                      "resources/rustediron1-alt2-bl/rustediron2_normal.png");
        qb_image_load(&attr.metallic_map, &image_attr,
                      "resources/rustediron1-alt2-bl/rustediron2_metallic.png");
        qb_image_load(&attr.roughness_map, &image_attr,
                      "resources/rustediron1-alt2-bl/rustediron2_roughness.png");
      }
      attr.albedo = { 1, 1, 1 };
      attr.metallic = 8.0f;
      attr.roughness = 0.5;      
      qb_material_create(&material, &attr, "ball");
    }

    qbTransform_ t = {
      glm::vec3(0, 0, 0),
      glm::vec3(0.5, -5, 0),
      glm::mat4(1)
    };

    qbRenderable r = qb_draw_sphere(2, 50, 50);

    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_renderable(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &material);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&block, attr);
    qb_entityattr_destroy(&attr);
  }
  {
    qbMaterial material;
    {
      qbMaterialAttr_ attr = {};
      attr.emission = { 1, 1, 1 };
      qb_material_create(&material, &attr, "ball");
    }


    std::vector<glm::vec3> positions = {
      { 0, 0, 5 },
      { 25, 0, 5 },
      { -25, 0, 5 },
      { 0, 25, 5 },
      { 0, -25, 5 },
    };

    for (auto&& pos : positions) {
      qbTransform_ t = {
        glm::vec3(0, 0, 0),
        pos,
        glm::mat4(1)
      };

      qbRenderable r = qb_draw_cube(1, 1, 1);

      qbEntity unused;
      qbEntityAttr attr;
      qb_entityattr_create(&attr);
      qb_entityattr_addcomponent(attr, qb_renderable(), &r);
      qb_entityattr_addcomponent(attr, qb_material(), &material);
      qb_entityattr_addcomponent(attr, qb_transform(), &t);
      qb_entity_create(&unused, attr);
      qb_entityattr_destroy(&attr);
    }
  }
  {
    qbMaterial material;
    {
      qbMaterialAttr_ attr = {};
      attr.albedo = { 1, 1, 1 };
      attr.metallic = 1.10;
      attr.roughness = 0.005;
      qb_material_create(&material, &attr, "ball");
    }

    qbTransform_ t = {
      glm::vec3(-50, -50, -5),
      glm::vec3(0, 0, 0),
      glm::mat4(1)
      //glm::rotate(glm::mat4(1), -3.141592f / 2.0f, glm::vec3(1, 0, 0))
    };

    qbRenderable r = qb_draw_rect(100, 100);

    qbEntity unused;
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_renderable(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &material);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&unused, attr);
    qb_entityattr_destroy(&attr);
  }

  qb_coro_sync([](qbVar v) {
    qbEntity block = v.i;
    qbTransform t;
    qb_instance_find(qb_transform(), block, &t);
    //t->orientation = glm::rotate(t->orientation, -3.14159f / 2.0f, glm::vec3(1, 1, 0));
    //t->position.y += 5.0f;
    int frame = 0;
    qbCamera camera = qb_camera_active();
    while (true) {
      t->orientation = glm::rotate(t->orientation, -0.001f, glm::vec3(0, 0, 1));

      //t->position.z = 0.0f + 5.0f*glm::cos((float)(frame) / 100.0f);
      //t->position.y = 0.0f - 5.0f*glm::sin((float)(frame) / 100.0f);

      if (qb_is_key_pressed(qbKey::QB_KEY_W)) {
        qb_camera_origin(camera, camera->origin + glm::vec3(glm::vec4(0.5, 0, 0, 1) * camera->rotation_mat));
      }

      if (qb_is_key_pressed(qbKey::QB_KEY_S)) {
        qb_camera_origin(camera, camera->origin + glm::vec3(glm::vec4(-0.5, 0, 0, 1) * camera->rotation_mat));
      }

      if (qb_is_key_pressed(qbKey::QB_KEY_Q)) {
        qb_camera_origin(camera, camera->origin + glm::vec3(glm::vec4(0, 0.5, 0, 1) * camera->rotation_mat));
      }

      if (qb_is_key_pressed(qbKey::QB_KEY_E)) {
        qb_camera_origin(camera, camera->origin + glm::vec3(glm::vec4(0, -0.5, 0, 1) * camera->rotation_mat));
      }

      if (qb_is_key_pressed(qbKey::QB_KEY_A)) {
        qb_camera_rotation(camera, glm::rotate(camera->rotation_mat, -0.025f, glm::vec3(0, 0, 1)));
      }

      if (qb_is_key_pressed(qbKey::QB_KEY_D)) {
        qb_camera_rotation(camera, glm::rotate(camera->rotation_mat, 0.025f, glm::vec3(0, 0, 1)));
      }

      ++frame;
      qb_coro_wait(0.01);
    }
    return qbNone;
  }, qbInt(block));

  qb_scene_reset();
}

void initialize_universe(qbUniverse* uni) {
  uint32_t width = 1200;
  uint32_t height = 800;
 
  qbUniverseAttr_ uni_attr = {};
  uni_attr.title = "Cubez example";
  uni_attr.width = width;
  uni_attr.height = height;
  uni_attr.enabled = qbFeature::QB_FEATURE_ALL;

  qbRendererAttr_ renderer_attr = {};
  uni_attr.create_renderer = qb_forwardrenderer_create;
  uni_attr.destroy_renderer = qb_forwardrenderer_destroy;
  uni_attr.renderer_args = &renderer_attr;

  qbAudioAttr_ audio_attr = {};
  audio_attr.sample_frequency = 44100;
  audio_attr.buffered_samples = 15;
  uni_attr.audio_args = &audio_attr;

  qb_init(uni, &uni_attr);
}

int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni = {};
  initialize_universe(&uni);
  qb_start();
  create_game();

  // https://www.reddit.com/r/gamedev/comments/6i39j2/tinysound_the_cutest_library_to_get_audio_into/

  qbLoopCallbacks_ loop_callbacks = {};
  qbLoopArgs_ loop_args = {};  
  while (qb_loop(&loop_callbacks, &loop_args) != QB_DONE) {
    qb_timing(uni, &timing_info);
  }
  return 0;
}