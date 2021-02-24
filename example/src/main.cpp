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
#include <string>
#include <cglm/struct.h>
#include <cubez/socket.h>
#include <cubez/struct.h>

#define NOMINMAX
#include <winsock.h>

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

qbGuiElement qb_guislider_create(vec2s pos, float width, const char* name) {
  qbGuiElement container;
  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { .1f, .1f, .1f, 1.f };

    qb_guielement_create(&container, "container", &attr);
    qb_guielement_setconstraint(container, QB_GUI_X, QB_CONSTRAINT_RELATIVE, pos.x + 15.f);
    qb_guielement_setconstraint(container, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, pos.y);
  }

  qbGuiElement line;
  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { .7f, .7f, .7f, 1.f };
    attr.radius = 1.f;

    qb_guielement_create(&line, "line", &attr);
    qb_guielement_setconstraint(line, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(line, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(line, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, width);
    qb_guielement_setconstraint(line, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 2.f);
  }

  qbGuiElement slider;
  {
    qbGuiElementCallbacks_ callbacks = {};
    callbacks.onfocus = [](qbGuiElement) { return true; };
    callbacks.ongetvalue = [](qbGuiElement b, qbVar) {
      double l = qb_guielement_getconstraint(b, QB_GUI_X, QB_CONSTRAINT_MIN);
      double r = qb_guielement_getconstraint(b, QB_GUI_X, QB_CONSTRAINT_MAX);
      double p = qb_guielement_getconstraint(b, QB_GUI_X, QB_CONSTRAINT_RELATIVE);
      double v = std::max(l, std::min(p, r));
      return qbDouble(v);
    };
    callbacks.onmove = [](qbGuiElement b, qbMouseMotionEvent e, int sx, int sy) {
      float x = qb_guielement_getconstraint(b, QB_GUI_X, QB_CONSTRAINT_RELATIVE);
      qb_guielement_setconstraint(b, QB_GUI_X, QB_CONSTRAINT_RELATIVE, x + (float)e->xrel);

      std::cout << qb_guielement_getvalue(b).d << std::endl;
      return true;
    };

    qbGuiElementAttr_ attr = {};
    attr.background_color = { 1.f, 1.f, 1.f, 1.f };
    attr.radius = 2.f;
    attr.callbacks = &callbacks;
    attr.offset = { -5.f, -5.f };

    qb_guielement_create(&slider, name, &attr);
    qb_guielement_setconstraint(slider, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(slider, QB_GUI_X, QB_CONSTRAINT_MIN, pos.x);
    qb_guielement_setconstraint(slider, QB_GUI_X, QB_CONSTRAINT_MAX, width);
    qb_guielement_setconstraint(slider, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(slider, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guielement_setconstraint(slider, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 10.f);
  }  
  qb_guielement_link(container, line);
  qb_guielement_link(container, slider);

  return container;
}

qbTiming_ timing_info;
qbScene main_menu_scene;
qbScene game_scene;
void create_main_menu() {
  qb_scene_create(&main_menu_scene, "Main Menu");
  qb_scene_set(main_menu_scene);

  qbGuiElementCallbacks_ window_callbacks = {};
  window_callbacks.onfocus = [](qbGuiElement) {
    std::cout << "onfocus\n";
    return true;
  };
  window_callbacks.onclick = [](qbGuiElement, qbMouseButtonEvent e) {
    std::cout << "onclick ( " << e->button << ", " << e->state << " )\n";
    return true;
  };
  window_callbacks.onscroll = [](qbGuiElement el, qbMouseScrollEvent e) {
    std::cout << "onscroll ( " << e->xrel << ", " << e->yrel << " )\n";

    if (qb_key_ispressed(qbKey::QB_KEY_LSHIFT)) {
      vec2s delta = { (float)(e->yrel * 5), 0.0f };
      qb_guielement_moveby(el, delta);
    } else {
      vec2s delta = { (float)e->xrel, (float)(e->yrel * 5) };
      qb_guielement_moveby(el, delta);
    }
    return true;
  };

  qbImage ball_image;
  {
    qbImageAttr_ attr;
    attr.type = QB_IMAGE_TYPE_2D;
    qb_image_load(&ball_image, &attr, "resources/soccer_ball.bmp");
  }

  std::vector<qbGuiElement> windows;
  qbGuiElement parent;

  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 0.0f, 0.0f, 0.0f, 0.05f };
    attr.callbacks = &window_callbacks;
   
    qb_guielement_create(&parent, "fps_parent", &attr);

    qb_guielement_setconstraint(parent, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, 512.f);
    qb_guielement_setconstraint(parent, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 64.f);
  }

  qbGuiElement text_box;
  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 1.f, 0.f, 0.f, 0.5f };
    attr.text_align = QB_TEXT_ALIGN_LEFT;
    attr.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    attr.text_size = 48;
    qb_guielement_create(&text_box, "text_box", &attr);

    qb_guielement_link(parent, text_box);
    qb_guielement_setconstraint(text_box, QB_GUI_X, QB_CONSTRAINT_PIXEL, 16.f);
    qb_guielement_setconstraint(text_box, QB_GUI_Y, QB_CONSTRAINT_PIXEL, 120.0f);
    qb_guielement_setconstraint(text_box, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, 512.0f);
    qb_guielement_setconstraint(text_box, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 64.0f);
  }
  qb_guielement_movetoback(parent);
  qb_guielement_movetofront(text_box);

  qbGuiElement main_menu;
  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 0.2f, 0.2f, 0.2f, 0.99f };
    attr.radius = 10.f;

    qbGuiElementCallbacks_ callbacks = {};
    callbacks.onfocus = [](qbGuiElement b) {
      std::cout << "ONFOCUS MAIN_MENU\n";
      return true;
    };

    attr.callbacks = &callbacks;

    qb_guielement_create(&main_menu, "main_menu", &attr);
    qb_guielement_resizeto(main_menu, { 250.f, 0.f });

    qb_guielement_setconstraint(main_menu, QB_GUI_X, QB_CONSTRAINT_MIN, 0.f);
    qb_guielement_setconstraint(main_menu, QB_GUI_Y, QB_CONSTRAINT_MIN, 0.f);
    qb_guielement_setconstraint(main_menu, QB_GUI_WIDTH, QB_CONSTRAINT_MIN, 50.f);
    qb_guielement_setconstraint(main_menu, QB_GUI_HEIGHT, QB_CONSTRAINT_MIN, 50.f);
    qb_guielement_setconstraint(main_menu, QB_GUI_HEIGHT, QB_CONSTRAINT_RELATIVE, -150.f);
    
    windows.push_back(main_menu);
  }

  qbGuiElement toolbar;
  {
    qbGuiElementCallbacks_ callbacks = {};
    callbacks.onmove = [](qbGuiElement b, qbMouseMotionEvent e, int sx, int sy) {

      qbGuiElement parent = qb_guielement_find("main_menu");
      qb_guielement_moveby(parent, { (float)e->xrel, (float)e->yrel });
      return true;
    };

    qbGuiElementAttr_ attr = {};
    attr.background_color = { 1.f, 0.f, 0.f, 0.5f };
    attr.callbacks = &callbacks;

    qb_guielement_create(&toolbar, "toolbar", &attr);
    qb_guielement_link(main_menu, toolbar);
    qb_guielement_setconstraint(toolbar, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(toolbar, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(toolbar, QB_GUI_WIDTH, QB_CONSTRAINT_PERCENT, 1.f);
    qb_guielement_setconstraint(toolbar, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 30.f);

    windows.push_back(toolbar);
  }

  qbGuiElement body;
  {    
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 0.f, 1.f, 0.f, 0.5f };

    qb_guielement_create(&body, "body", &attr);
    qb_guielement_link(main_menu, body);
    qb_guielement_setconstraint(body, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(body, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 30.f);
    qb_guielement_setconstraint(body, QB_GUI_WIDTH, QB_CONSTRAINT_PERCENT, 1.f);
    qb_guielement_setconstraint(body, QB_GUI_HEIGHT, QB_CONSTRAINT_RELATIVE, -30.f);

    windows.push_back(body);
  }

  qbGuiElement red_button;
  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 1.0f, 0.35f, 0.35f, 1.f };
    attr.radius = 2.f;

    qb_guielement_create(&red_button, "close", &attr);
    qb_guielement_link(toolbar, red_button);
    qb_guielement_setconstraint(red_button, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guielement_setconstraint(red_button, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guielement_setconstraint(red_button, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guielement_setconstraint(red_button, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 10.f);

    windows.push_back(red_button);
  }

  qbGuiElement yellow_button;
  {
    qbGuiElementCallbacks_ callbacks = {};
    callbacks.onclick = [](qbGuiElement w, qbMouseButtonEvent e) {
      qb_guielement_close(qb_guielement_find("body"));

      qbGuiElement main_menu = qb_guielement_find("main_menu");
      qb_guielement_setconstraint(main_menu, QB_GUI_HEIGHT, QB_CONSTRAINT_MAX, 30.f);
      return true;
    };

    qbGuiElementAttr_ attr = {};
    attr.background_color = { 1.0f, 1.0f, 0.35f, 1.f };
    attr.radius = 2.f;
    attr.callbacks = &callbacks;

    qb_guielement_create(&yellow_button, "minimize", &attr);
    qb_guielement_link(toolbar, yellow_button);
    qb_guielement_setconstraint(yellow_button, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 25.f);
    qb_guielement_setconstraint(yellow_button, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guielement_setconstraint(yellow_button, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guielement_setconstraint(yellow_button, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 10.f);

    windows.push_back(yellow_button);
  }

  qbGuiElement green_button;
  {
    qbGuiElementCallbacks_ callbacks = {};
    callbacks.onclick = [](qbGuiElement w, qbMouseButtonEvent e) {
      qb_guielement_open(qb_guielement_find("body"));

      qbGuiElement main_menu = qb_guielement_find("main_menu");
      qb_guielement_clearconstraint(main_menu, QB_GUI_HEIGHT, QB_CONSTRAINT_MAX);
      return true;
    };

    qbGuiElementAttr_ attr = {};
    attr.background_color = { 0.35f, 1.0f, 0.35f, 1.f };
    attr.radius = 2.f;
    attr.callbacks = &callbacks;

    qb_guielement_create(&green_button, "maximize", &attr);
    qb_guielement_link(toolbar, green_button);
    qb_guielement_setconstraint(green_button, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 40.f);
    qb_guielement_setconstraint(green_button, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guielement_setconstraint(green_button, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guielement_setconstraint(green_button, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, 10.f);

    windows.push_back(green_button);
  }

  qbGuiElement display;
  {
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 0.35f, 0.35f, 0.35f, 1.f };
    attr.radius = 2.f;

    qb_guielement_create(&display, "display", &attr);
    qb_guielement_link(body, display);
    qb_guielement_setconstraint(display, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guielement_setconstraint(display, QB_GUI_Y, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guielement_setconstraint(display, QB_GUI_WIDTH, QB_CONSTRAINT_RELATIVE, -20.f);
    qb_guielement_setconstraint(display, QB_GUI_HEIGHT, QB_CONSTRAINT_RELATIVE, -20.f);

    windows.push_back(display);
  }

  qbGuiElement slider = qb_guislider_create({ 0.f, 500.f }, 200.f, "slider1");
  qb_guielement_link(display, slider);  
  //qb_guielement_link(display, qb_guislider_create({ 0.f, 300.f }, 200.f, "slider2"));

#if 0
  if (1)
  {
    qbTextboxAttr_ attr = {};
    attr.align = QB_TEXT_ALIGN_LEFT;
    attr.text_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    char16_t* lorem_ipsum = uR"(Lorem ipsum dolor sit amet, eos duis illud ullamcorper ea.Est cu tritani inermis.Pro dicta paulo consequat cu, ei integre suavitate vituperatoribus sed, vel ne enim agam disputationi.Ut ius noster blandit referrentur, nusquam cotidieque vim ut.At usu tritani consetetur.Movet populo praesent at sed, no eos appellantur deterruisset.

Vis an quem clita, his id facete malorum persius.Solum populo elaboraret ei mei.Wisi diceret fabulas mel in, graeci vidisse noluisse quo ex.Vis dolorum evertitur at.

Pro elitr malorum voluptaria ex, ius elitr praesent accommodare ei.An quem noster reprimique mei.Tempor docendi duo ad, cu movet quando suscipit sed.Eos legere facilisi ut, eu sit ancillae constituam.Cum movet nobis ei, cu sea dolor copiosae suscipiantur.Commune eligendi moderatius ne nec, mea eu aperiri deserunt qualisque, ferri dicant albucius te qui.

Pri affert decore eu, ad movet vidisse has.Qui errem melius iudicabit et.Ea sanctus oporteat usu, ad velit lobortis splendide vis, justo solet principes pro eu.Ei mei sanctus consectetuer.Debet vocent qui ut.Mel tota inermis reprimique te, sea oblique consequat in.

Dicit nihil oportere vix id, pri te tempor alterum.Ea vel etiam inciderint.Qualisque definitionem ea eam, quo accusamus vituperata ut.Eos at posse populo numquam, graeco adipisci mediocrem vim te.His ne erat molestiae.
      )";

    qbGuiElement new_game_text;
    qb_textbox_create(&new_game_text, &attr, "lorem_ipsum", { 800.0f, 64.0f }, 8, lorem_ipsum);

    qb_guielement_link(display, new_game_text);
    qb_guiconstraint_x(new_game_text, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_y(new_game_text, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_width(new_game_text, QB_CONSTRAINT_RELATIVE, -20.f);

    windows.push_back(new_game_text);
  }
#endif

  qb_guielement_close(main_menu);
  qb_guielement_open(main_menu);

  qb_scene_attach(main_menu_scene, "main_menu", new std::vector<qbGuiElement>(std::move(windows)));
  qb_coro_sync([](qbVar text) {
    qbGuiElement text_box = (qbGuiElement)text.p;
    while (true) {
      int fps = (int)timing_info.render_fps;
      {
        std::string text_fps = std::to_string(fps);
        while (text_fps.length() < 4) {
          text_fps.insert(text_fps.begin(), '0');
        }
        text_fps = std::string("FPS: ") + text_fps;
        qb_guielement_settext(text_box, text_fps.data());
      }
      qb_coro_waitframes(10);
    }

    return qbNil;
  }, qbPtr(text_box));

  {
    const int count = 9;
    const float size = 64.f;

    qbGuiElement actionbar_container;
    {
      qbGuiElementAttr_ attr = {};
      attr.background_color = { 0.f, 0.f, 0.f, 1.f };

      qb_guielement_create(&actionbar_container, "actionbar_container", &attr);

      qb_guielement_setconstraint(actionbar_container, QB_GUI_X, QB_CONSTRAINT_PERCENT, 0.5f);
      qb_guielement_setconstraint(actionbar_container, QB_GUI_X, QB_CONSTRAINT_RELATIVE, 0.5f);
      qb_guielement_setconstraint(actionbar_container, QB_GUI_Y, QB_CONSTRAINT_PERCENT, 0.f);

      qb_guielement_setconstraint(actionbar_container, QB_GUI_WIDTH, QB_CONSTRAINT_PERCENT, 1.f);
      qb_guielement_setconstraint(actionbar_container, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, size);
    }

    qbGuiElement action_bar[count];
    qbGuiElementAttr_ attr = {};
    attr.background_color = { 0.9f, 0.9f, 0.9f, 0.9f };
    attr.radius = 2.0f;
    attr.offset = { -size * 0.5f, -size * 0.5f };

    for (int i = 0; i < count; ++i) {
      qb_guielement_create(&action_bar[i], (std::string("actionbar") + std::to_string(i)).data(), &attr);

      qbGuiElement b = action_bar[i];
      qb_guielement_setconstraint(b, QB_GUI_X, QB_CONSTRAINT_PIXEL, size * i + 32.f);
      qb_guielement_setconstraint(b, QB_GUI_Y, QB_CONSTRAINT_PIXEL, 500.f);

      qb_guielement_setconstraint(b, QB_GUI_WIDTH, QB_CONSTRAINT_PIXEL, size);
      qb_guielement_setconstraint(b, QB_GUI_HEIGHT, QB_CONSTRAINT_PIXEL, size);
    }
  }

  qb_guielement_movetofront(slider);
  qb_scene_reset();
}

qbEntity ship_entity;

void create_game() {
  qb_scene_create(&game_scene, "Game");
  qb_scene_set(game_scene);

  qbCamera main_camera;
  {
    qbCameraAttr_ attr = {};
    attr.fov = glm_rad(45.0f);
    attr.height = 800;
    attr.width = 1200;
    attr.znear = 0.1f;
    attr.zfar = 1000;
    attr.origin = vec3s{ -10, 0, 0 };
    attr.rotation_mat = GLMS_MAT4_IDENTITY_INIT;
    qb_camera_create(&main_camera, &attr);
    qb_camera_activate(main_camera);
  }
  create_main_menu();

  {
    qb_light_directional(0, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, 0.05f);
    qb_light_directional(1, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0.1f);
    qb_light_enable(0, qbLightType::QB_LIGHT_TYPE_DIRECTIONAL);
    //qb_light_enable(1, qbLightType::QB_LIGHT_TYPE_DIRECTIONAL);
  }

  qb_coro_sync(print_timing_info, qbPtr(&timing_info));
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
                      "resources/rustediron1-alt2-bl/rustediron2_basecolor.png");
        qb_image_load(&attr.normal_map, &image_attr,
                      "resources/rustediron1-alt2-bl/rustediron2_normal.png");
        qb_image_load(&attr.metallic_map, &image_attr,
                      "resources/rustediron1-alt2-bl/rustediron2_metallic.png");
        qb_image_load(&attr.roughness_map, &image_attr,
                      "resources/rustediron1-alt2-bl/rustediron2_roughness.png");
        qb_image_load(&attr.emission_map, &image_attr,
                    "resources/rustediron1-alt2-bl/rustediron2_roughness.png");
      }
      attr.emission = { -0.5f, 0.0f, 0.0f };
      qb_material_create(&material, &attr, "ball");
    }

    qbTransform_ t = {
      vec3s{0.0f, 0.0f, 0.0f},
      vec3s{0.0f, 0.0f, -3.0f},
      GLMS_MAT4_IDENTITY_INIT
    };
    
    qbModel model = qb_model_load("", "resources/models/destroyer_textured.obj"); // qb_draw_sphere(2, 50, 50);
    qbModelGroup r = qb_model_upload(model);

    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_modelgroup(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &material);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entityattr_addcomponent(attr, qb_collider(), model->colliders[0]);
    qb_entity_create(&ship_entity, attr);
    qb_entityattr_destroy(&attr);
  }
  {
    std::vector<vec3s> positions = {
      { 0, 0, 5 },
      { 25, 0, 5 },
      { -25, 0, 5 },
      { 0, 25, 5 },
      { 0, -25, 5 },
    };
    std::vector<vec3s> colors = {
      { 1.0, 1.0, 1.0 },
      { 1.0, 1.0, 1.0 },
      { 1.0, 1.0, 1.0 },
      { 1.0, 1.0, 1.0 },
      { 1.0, 1.0, 1.0 },
    };

    for (size_t i = 0; i < positions.size(); ++i) {
      vec3s& pos = positions[i];
      vec3s& color = colors[i];
      qb_light_point(i, color, pos, 15.0f, 200.0f);
      qb_light_enable(i, qbLightType::QB_LIGHT_TYPE_POINT);


      qbMaterial material;
      {
        qbMaterialAttr_ attr = {};
        attr.emission = color;
        attr.albedo = color;
        qb_material_create(&material, &attr, "ball");
      }

      qbTransform_ t = {
        vec3s{0, 0, 0},
        pos,
        GLMS_MAT4_IDENTITY_INIT
      };

      qbModelGroup r = qb_draw_cube(1, 1, 1, QB_DRAW_MODE_TRIANGLES, nullptr);

      qbEntity unused;
      qbEntityAttr attr;
      qb_entityattr_create(&attr);
      qb_entityattr_addcomponent(attr, qb_modelgroup(), &r);
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
      attr.albedo = { .5f, .5f, .5f };
      attr.metallic = 1.5f;
      attr.roughness = 0.0f;
      attr.emission = { 0.0f, 0.0f, 0.0f };
      qb_material_create(&material, &attr, "ball");
    }

    qbTransform_ t = {
      vec3s{0, 0, 0},
      vec3s{ 0, 0, 45 },
      GLMS_MAT4_IDENTITY_INIT
    };

    qbModelGroup r = qb_draw_cube(-100, -100, -100, QB_DRAW_MODE_TRIANGLES, nullptr);

    qbEntity unused;
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_modelgroup(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &material);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&unused, attr);
    qb_entityattr_destroy(&attr);
  }


  qb_coro_sync([](qbVar v) {
    qbEntity block = v.i;
    qbTransform t;
    qb_instance_find(qb_transform(), block, &t);
    //t->orientation = rotate(t->orientation, -3.14159f / 2.0f, vec3s(1, 1, 0));
    //t->position.y += 5.0f;
    int frame = 0;
    qbCamera camera = qb_camera_getactive();
    int mouse_x, mouse_y;
    qb_mouse_getposition(&mouse_x, &mouse_y);
    float cam_dir = 0.0f, cam_zdir = 0.0f;
    float cam_dis = 20.0f;
    float cam_dis_dt = 0.0f;
    while (true) {
      t->orientation = glms_rotate(t->orientation, -0.001f, vec3s{ 0.0f, 0.0f, 1.0f });

      //t->position.z = 0.0f + 5.0f*cos((float)(frame) / 100.0f);
      //t->position.y = 0.0f - 5.0f*sin((float)(frame) / 100.0f);

      int x, y;
      qb_mouse_getrelposition(&x, &y);

      float dx, dy;
      dx = (float)x * -0.01f;
      dy = (float)y * 0.01f;

      float speed = 0.5f;
      if (qb_key_ispressed(qbKey::QB_KEY_LSHIFT)) {
        speed *= 10.0f;
      }
      if (qb_key_ispressed(qbKey::QB_KEY_LCTRL)) {
        speed *= 0.1f;
      }

      int wx, wy;
      qb_mouse_getwheel(&wx, &wy);
      if (wx != 0 || wy != 0) {
        std::cout << wx << ", " << wy << std::endl;
      }

      if (qb_mouse_getrelative()) {
        mat4s m = camera->rotation_mat;
        m = glms_rotate(m, dx, vec3s{ 0, 0, 1 });
        m = glms_rotate(m, -dy, vec3s{ 0, 1, 0 });
        qb_camera_setrotation(camera, m);

        /*cam_dis += cam_dis_dt;

        vec4s orig = glms_mat4_mulv(m, vec4s{ -cam_dis, 0.f, 0.f, 1.0f });
        qb_camera_setorigin(camera, { orig.x, orig.y, orig.z });
        cam_dis_dt *= 0.9f;*/
      }

      static uint32_t was_pressed = 0;
      if (qb_key_ispressed(QB_KEY_F11) && !was_pressed) {
        was_pressed = 1;
        if (qb_window_fullscreen() == QB_WINDOWED) {
          qb_window_setfullscreen(QB_WINDOW_FULLSCREEN_DESKTOP);
        } else {
          qb_window_setfullscreen(QB_WINDOWED);
        }
      } else if (!qb_key_ispressed(QB_KEY_F11)) {
        was_pressed = 0;
      }

      if (qb_key_ispressed(qbKey::QB_KEY_W)) {
        /*if (qb_key_ispressed(qbKey::QB_KEY_LSHIFT)) {
          cam_dis_dt = -0.1f;
        } else {
          cam_dis_dt = -0.01f;
        }*/
        vec4s dir = { -speed, 0.0f, 0.0f, 1.0f };
        qb_camera_setorigin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_S)) {
        /*if (qb_key_ispressed(qbKey::QB_KEY_LSHIFT)) {
          cam_dis_dt = 0.1f;
        } else {
          cam_dis_dt = 0.01f;
        }*/
        
        vec4s dir = { speed, 0.0f, 0.0f, 1.0f };
        qb_camera_setorigin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
          
      }

      if (qb_key_ispressed(qbKey::QB_KEY_A)) {
        vec4s dir = { 0.0f, -speed, 0.0f, 1.0f };
        qb_camera_setorigin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_D)) {
        vec4s dir = { 0.0f, speed, 0.0f, 1.0f };
        qb_camera_setorigin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_Q)) {
        qb_camera_setrotation(camera, glms_rotate(camera->rotation_mat, 0.025f, vec3s{ 1, 0, 0 }));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_E)) {
        qb_camera_setrotation(camera, glms_rotate(camera->rotation_mat,  -0.025f, vec3s{ 1, 0, 0 }));
      }

      /*

      int x, y;
      qb_mouse_getrelposition(&x, &y);

      float dx, dy;
      dx = (float)x * -0.001f;
      dy = (float)y * 0.001f;
      cam_dir += dx;
      cam_zdir += dy;
      cam_zdir = glm_clamp(cam_zdir, glm_rad(-90.0f), glm_rad(90.0f));
      {
        mat4s m = GLMS_MAT4_IDENTITY_INIT;
        m = glms_rotate(m, cam_dir, vec3s{ 0, 0, 1 });
        m = glms_rotate(m, cam_zdir, vec3s{ 0, 1, 0 });
        qb_camera_setrotation(camera, m);
      }
      
      vec4s dir = {};
      if (qb_key_ispressed(qbKey::QB_KEY_W)) {
        dir = { 0.5f, 0.0f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_setorigin(camera, glms_vec3_add(camera->origin, d));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_S)) {
        dir = { -0.5f, 0.0f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_setorigin(camera, glms_vec3_add(camera->origin, d));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_A)) {
        dir = { 0.0f, 0.5f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_setorigin(camera, glms_vec3_add(camera->origin, d));
      }

      if (qb_key_ispressed(qbKey::QB_KEY_D)) {
        dir = { 0.0f, -0.5f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_setorigin(camera, glms_vec3_add(camera->origin, d));
      }*/

      ++frame;
      qb_coro_wait(0.01);
    }
    return qbNil;
  }, qbInt(ship_entity));

  qb_scene_reset();
}

void initialize_universe(qbUniverse* uni) {
  uint32_t width = 1200;
  uint32_t height = 800;
 
  qbUniverseAttr_ uni_attr = {};
  uni_attr.title = "Cubez example";
  uni_attr.width = width;
  uni_attr.height = height;

  qbRendererAttr_ renderer_attr = {};
  renderer_attr.create_renderer = qb_forwardrenderer_create;
  renderer_attr.destroy_renderer = qb_forwardrenderer_destroy;
  uni_attr.renderer_args = &renderer_attr;

  qbAudioAttr_ audio_attr = {};
  audio_attr.sample_frequency = 44100;
  audio_attr.buffered_samples = 15;
  uni_attr.audio_args = &audio_attr;

  qbScriptAttr_ script_attr = {};
  char* dir[] = { ".", "scripts", 0 };
  char* entrypoint = "main.lua";
  script_attr.directory = dir;
  script_attr.entrypoint = entrypoint;
  uni_attr.script_args = &script_attr;

  qb_init(uni, &uni_attr);
}

qbVar test_collision(qbVar v) {
  qbMaterial red;
  {
    qbMaterialAttr_ attr = {};
    attr.albedo = { 1.f, 0.f, 0.f };
    qb_material_create(&red, &attr, "ball");
  }

  qbMaterial green;
  {
    qbMaterialAttr_ attr = {};
    attr.albedo = { 0.f, 1.f, 0.f };
    qb_material_create(&green, &attr, "ball");
  }

  qbCollider a, b, c;
  qbEntity a_block, b_block, c_block;
  {
    qbTransform_ t = {
      vec3s{ 0.f, 0.f, 0.f },
      vec3s{ 2.5f, 0.f, 0.f },
      GLMS_MAT4_IDENTITY_INIT
    };

    qbModelGroup r = qb_draw_cube(2, 2, 2, QB_DRAW_MODE_TRIANGLES, &a);

    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_modelgroup(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &red);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&a_block, attr);
    qb_entityattr_destroy(&attr);
  }

  {
    qbTransform_ t = {
      vec3s{ 0.f, 0.f, 0.f },
      vec3s{ 0.f, 0.f, 0.f },
      glms_euler_xyz(vec3s{ 0.f, 0.f, glm_rad(45.f) })
    };

    qbModelGroup r = qb_draw_cube(2, 2, 2, QB_DRAW_MODE_TRIANGLES, &b);
    
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_modelgroup(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &green);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&b_block, attr);
    qb_entityattr_destroy(&attr);
  }

  {
    qbTransform_ t = {
      vec3s{ 0.f, 0.f, 0.f },
      vec3s{ 0.f, 0.f, 0.f },
      GLMS_MAT4_IDENTITY_INIT
    };

    qbModelGroup r = qb_draw_sphere(2, 50, 50, QB_DRAW_MODE_TRIANGLES, &c);

    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, qb_modelgroup(), &r);
    qb_entityattr_addcomponent(attr, qb_material(), &green);
    qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&c_block, attr);
    qb_entityattr_destroy(&attr);
  }

  float rot = 0.f;
  float dis = 10.f;
  while (true) {
    if (!qb_camera_getactive()) {
      qb_coro_yield(qbNil);
      continue;
    }

    qbTransform a_t, b_t, c_t, ship_t;
    qbCollider ship_collider;
    qb_instance_find(qb_transform(), a_block, &a_t);
    qb_instance_find(qb_transform(), b_block, &b_t);
    qb_instance_find(qb_transform(), c_block, &c_t);
    qb_instance_find(qb_transform(), ship_entity, &ship_t);
    qb_instance_find(qb_collider(), ship_entity, &ship_collider);

    a_t->orientation = glms_euler_xyz(vec3s{ 0.f, glm_rad(rot), 0.f });
    b_t->orientation = glms_euler_xyz(vec3s{ 0.f, 0.f, glm_rad(1.5f * rot) });

    qbMaterial* material;
    qb_instance_find(qb_material(), a_block, &material);
    if (qb_collider_check(a, b, a_t, b_t)) {
      (*material)->albedo = { 1.f, 0.0f, 0.f };
    } else {
      (*material)->albedo = { 0.f, 1.f, 0.f };
    }

    int mouse_x, mouse_y;
    qb_mouse_getposition(&mouse_x, &mouse_y);
    vec3s mouse_dir = qb_camera_screentoworld(qb_camera_getactive(), { (float)mouse_x, (float)mouse_y });
    qbRay_ mouse_ray{ qb_camera_getactive()->origin, mouse_dir };

    c_t->position = glms_vec3_add(qb_camera_getactive()->origin, glms_vec3_scale(mouse_dir, dis));

    float t = 0.f;
    int scroll_x, scroll_y;
    qb_mouse_getwheel(&scroll_x, &scroll_y);
    dis += (float)scroll_y;

    /*if (qb_collider_checkray(a, a_t, &mouse_ray, &t)) {
      (*material)->albedo = { 1.f, 0.0f, 0.f };
    } else {
      (*material)->albedo = { 0.f, 1.f, 0.f };
    }

    qb_instance_find(qb_material(), ship_entity, &material);    
    if (qb_collider_checkray(ship_collider, ship_t, &mouse_ray, &t)) {
      (*material)->albedo = { 1.f, 0.0f, 0.f };
    } else {
      (*material)->albedo = { 0.f, 1.f, 0.f };
    }*/

    rot += 0.1f;
    
    //qb_coro_waitframes(1);
    qb_coro_wait(0.01);
  }
}

int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni = {};
  initialize_universe(&uni);
  qb_start();
  create_game();

  struct TestInner {
    int val;
  };

  struct TestC {
    int a;
    TestInner b;
  };

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);

    qb_componentattr_onpack(attr,
      [](qbComponent, qbEntity, const void* read, qbBuffer_* write, ptrdiff_t* pos) {
        TestC* t = (TestC*)read;
        
        qb_buffer_write(write, pos, sizeof(int), &t->a);
        qb_buffer_write(write, pos, sizeof(int), &t->b.val);
      });

    qb_componentattr_onunpack(attr,
      [](qbComponent, qbEntity, void* write, const qbBuffer_* read, ptrdiff_t* pos) {
        TestC* t = (TestC*)write;
        memset(t, 0, sizeof(TestC));
        
        qb_buffer_read(read, pos, sizeof(int), &t->a);
        qb_buffer_read(read, pos, sizeof(int), &t->b.val);
      });

    qb_componentattr_destroy(&attr);
  }

  qb_mouse_setrelative(0);

#if 0
  {
    uint8_t buf_[150] = { 0 };
    qbBuffer buf{ sizeof(buf_), buf_ };
    
    ptrdiff_t pos = 0;
    qbVar a = qbDouble(1.125);
    qb_buffer_write(&buf, &pos, a.size, a.bytes);


    pos = 0;
    qbVar b = qbDouble(0.);

    qb_buffer_read(&buf, &pos, b.size, &b.bytes);
    std::cout << b.d << std::endl;

    for (qbIterator_ it = qb_iterator_begin(qb_transform()); !qb_iterator_isdone(&it); qb_iterator_next(&it)) {
      qbTransform t = (qbTransform)qb_iterator_at(&it);
    }
  }
  {
    qbVar array = qbArray(QB_TAG_INT);
    qb_array_append(array, qbInt(10));

    std::cout << qb_array_at(array, -1)->i << std::endl;
  }

  {
    qbVar map = qbMap(QB_TAG_STRING, QB_TAG_INT);
    qb_map_insert(map, qbString("Hello, World!"), qbInt(10));

    std::cout << qb_map_at(map, qbString("Hello, World!"))->i << std::endl;
  }

#endif
  {
    qbEntityAttr attr;
    qb_entityattr_create__unsafe(&attr);
  }


  qb_coro_async([](qbVar) {
    qbSocket server;
    {
      qbSocketAttr_ attr{};
      attr.np = QB_TCP;
      attr.af = QB_IPV4;
      qb_socket_create(&server, &attr);
    }

    qbEndpoint_ endpoint{};
    qb_endpoint(&endpoint, "127.0.0.1", 12346);

    qbResult err = qb_socket_connect(server, &endpoint);
    while (err == QB_ERROR_SOCKET && errno == QB_SOCKET_ECONNREFUSED) {
      qb_coro_wait(0.5);
      err = qb_socket_connect(server, &endpoint);
    }
    const size_t capacity = 1 << 16;
    qbBuffer_ read_buf{ capacity, new uint8_t[capacity] };
    qbBuffer_ process_buf{ capacity, new uint8_t[capacity] };

    qbSchema pos_schema = qb_schema_find("Position");
    void* s = alloca(qb_struct_size(pos_schema));
    memset(s, 0, qb_struct_size(pos_schema));

    int32_t read_bytes = 0;

    qbVar received = qbMap(QB_TAG_INT, QB_TAG_ANY);
    for (int i = 0; i < 200; ++i) {            
      // Pump the socket until we have the header.
      while (read_bytes < sizeof(int32_t)) {
        read_bytes += qb_socket_recv(
          server, (char*)read_buf.bytes + read_bytes,
          read_buf.capacity - read_bytes, 0);
      }

      // Get the size of the packet we expect.
      ptrdiff_t pos = 0;
      int32_t packet_size;
      size_t read = qb_buffer_readl(&read_buf, &pos, &packet_size);
      if (read != sizeof(int32_t)) {
        std::cout << "Invalid packet: cannot read packet size\n";
        break;
      }

      // Pump the socket until we have a full packet (but we may read more than
      // one buffered packet).
      while (read_bytes < packet_size) {
        read_bytes += qb_socket_recv(
          server, (char*)read_buf.bytes + read_bytes,
          read_buf.capacity - read_bytes, 0);
      }
      
      // Copy the read packet to be processed. This can be modified to be async
      // instead: copy the read_buf into the process_buf, then continue reading
      // from the socket.
      read_bytes -= packet_size;
      memcpy(process_buf.bytes, read_buf.bytes, packet_size);
      memcpy(read_buf.bytes, read_buf.bytes + packet_size, read_bytes);

      // Now we have one full packet in the process_buf.
      qbVar v;
      read = qb_var_unpack(&v, &process_buf, &pos);
      if (read == 0) {
        std::cout << "Could not unpack" << std::endl;
      } else {
        qbVar str = qb_var_tostring(v);
        
        std::cout << "Got from server: " << str.s << std::endl;
        qb_var_destroy(&str);
        qb_var_destroy(&v);
      } 

      if (!qb_running()) {
        return qbNil;
      }

      //std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return qbNil;
  }, qbNil);

  qb_coro_async([](qbVar) {
    qbSocket server;
    {
      qbSocketAttr_ attr{};
      attr.np = QB_TCP;
      attr.af = QB_IPV4;
      qb_socket_create(&server, &attr);
    }

    qbEndpoint_ endpoint{};
    qb_endpoint(&endpoint, INADDR_ANY, 12346);

    int opt = 1;
    qb_socket_setopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    qb_socket_bind(server, &endpoint);
    qb_socket_listen(server, 0);

    qbSocket client{};
    qb_socket_accept(server, &client, &endpoint);

    qbSchema pos_schema = qb_schema_find("Position");
    void* s = alloca(qb_struct_size(pos_schema));
    memset(s, 0, qb_struct_size(pos_schema));
    
    for (int i = 0; i < 100; ++i) {
      uint8_t buffer[128];
      ptrdiff_t pos = 0;
      qbBuffer_ buf{ sizeof(buffer), buffer };

      qbVar v = qbStruct(pos_schema, s);
      *qb_struct_ati(v, 0) = qbInt(0);
      *qb_struct_ati(v, 1) = qbInt(i);

      size_t written = 0;
      written += qb_buffer_writel(&buf, &pos, 0);
      written += qb_var_pack(v, &buf, &pos);
      
      pos = 0;
      qb_buffer_writel(&buf, &pos, written);
      //std::cout << "Wrote " << written << " bytes\n";

      qb_socket_send(client, (char*)buffer, written, 0);

      if (!qb_running()) {
        return qbNil;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return qbNil;
  }, qbNil);

  //qb_coro_sync(test_collision, qbNil);

  uint8_t buf[128];
  qbBuffer_ buffer{ sizeof(buf), buf };

  qbVar v = qbMap(QB_TAG_INT, QB_TAG_ANY);
  qb_map_insert(v, qbInt(97), qbInt(10));
  qb_map_insert(v, qbInt(112), qbString("HElllooo, world"));

  qbVar before = v;
  qbVar after = qbNil;

  ptrdiff_t pos = 0;
  qb_var_pack(before, &buffer, &pos);
  pos = 0;

  qb_var_unpack(&after, &buffer, &pos);
  std::cout << qb_map_at(v, qbInt(97))->i << ", " << qb_map_at(v, qbInt(112))->s << std::endl;

  qbLoopCallbacks_ loop_callbacks = {};
  qbLoopArgs_ loop_args = {};  
  while (qb_loop(&loop_callbacks, &loop_args) != QB_DONE) {
    qb_timing(uni, &timing_info);
  }
  return 0;
}