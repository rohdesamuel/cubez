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

qbGuiBlock qb_guislider_create(vec2s pos, float width, const char* name) {
  qbGuiBlock container;
  {
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { .1f, .1f, .1f, 1.f };

    qb_guiblock_create(&container, &attr, "container");
    qb_guiconstraint_x(container, QB_CONSTRAINT_RELATIVE, pos.x + 15.f);
    qb_guiconstraint_y(container, QB_CONSTRAINT_RELATIVE, pos.y);
  }

  qbGuiBlock line;
  {
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { .7f, .7f, .7f, 1.f };
    attr.radius = 1.f;

    qb_guiblock_create(&line, &attr, "line");
    qb_guiconstraint_x(line, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_y(line, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_width(line, QB_CONSTRAINT_PIXEL, width);
    qb_guiconstraint_height(line, QB_CONSTRAINT_PIXEL, 2.f);
  }

  qbGuiBlock slider;
  {
    qbGuiBlockCallbacks_ callbacks = {};
    callbacks.onfocus = [](qbGuiBlock) { return true; };
    callbacks.getvalue = [](qbGuiBlock b, qbVar) {
      double l = qb_guiconstraint_get(b, QB_CONSTRAINT_MIN, QB_GUI_X);
      double r = qb_guiconstraint_get(b, QB_CONSTRAINT_MAX, QB_GUI_X);
      double p = qb_guiconstraint_get(b, QB_CONSTRAINT_RELATIVE, QB_GUI_X);
      double v = std::max(l, std::min(p, r));
      return qbDouble(v);
    };
    callbacks.onmove = [](qbGuiBlock b, qbMouseMotionEvent e, int sx, int sy) {
      float x = qb_guiconstraint_get(b, QB_CONSTRAINT_RELATIVE, QB_GUI_X);
      qb_guiconstraint_x(b, QB_CONSTRAINT_RELATIVE, x + (float)e->xrel);

      std::cout << qb_guiblock_value(b).d << std::endl;
      return true;
    };

    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 1.f, 1.f, 1.f, 1.f };
    attr.radius = 2.f;
    attr.callbacks = &callbacks;
    attr.offset = { -5.f, -5.f };

    qb_guiblock_create(&slider, &attr, name);
    qb_guiconstraint_x(slider, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_x(slider, QB_CONSTRAINT_MIN, pos.x);
    qb_guiconstraint_x(slider, QB_CONSTRAINT_MAX, width);
    qb_guiconstraint_y(slider, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_width(slider, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guiconstraint_height(slider, QB_CONSTRAINT_PIXEL, 10.f);
  }  
  qb_guiblock_link(container, line);
  qb_guiblock_link(container, slider);

  return container;
}

qbTiming_ timing_info;
qbScene main_menu_scene;
qbScene game_scene;
void create_main_menu() {
  qb_scene_create(&main_menu_scene, "Main Menu");
  qb_scene_set(main_menu_scene);

  qbGuiBlockCallbacks_ window_callbacks = {};
  window_callbacks.onfocus = [](qbGuiBlock) {
    std::cout << "onfocus\n";
    return true;
  };
  window_callbacks.onclick = [](qbGuiBlock, qbMouseButtonEvent e) {
    std::cout << "onclick ( " << e->button << ", " << e->state << " )\n";
    return true;
  };
  window_callbacks.onscroll = [](qbGuiBlock block, qbMouseScrollEvent e) {
    std::cout << "onscroll ( " << e->xrel << ", " << e->yrel << " )\n";

    if (qb_key_pressed(qbKey::QB_KEY_LSHIFT)) {
      vec3s delta = { (float)(e->yrel * 5), 0.0f, 0.0f };
      qb_guiblock_moveby(block, delta);
    } else {
      vec3s delta = { (float)e->xrel, (float)(e->yrel * 5), 0.0f };
      qb_guiblock_moveby(block, delta);
    }
    return true;
  };

  qbImage ball_image;
  {
    qbImageAttr_ attr;
    attr.type = QB_IMAGE_TYPE_2D;
    qb_image_load(&ball_image, &attr, "resources/soccer_ball.bmp");
  }

  std::vector<qbGuiBlock> windows;
  qbGuiBlock parent;

  {
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.0f, 0.0f, 0.0f, 0.95f };
    attr.callbacks = &window_callbacks;
    qb_guiblock_create(&parent, &attr, "fps_parent");

    qb_guiconstraint_width(parent, QB_CONSTRAINT_PIXEL, 512.f);
    qb_guiconstraint_height(parent, QB_CONSTRAINT_PIXEL, 64.f);
  }

  qbGuiBlock text_box;
  {
    qbTextboxAttr_ textbox_attr = {};
    textbox_attr.align = QB_TEXT_ALIGN_LEFT;
    textbox_attr.text_color = { 1.0f, 0.0f, 0.0f, 1.0f };
    qb_textbox_create(&text_box, &textbox_attr, "text_box", { 512.f, 64.f }, 48, u"");

    qb_guiblock_link(parent, text_box);
    qb_guiconstraint_x(text_box, QB_CONSTRAINT_PIXEL, 16.f);
    qb_guiconstraint_y(text_box, QB_CONSTRAINT_PIXEL, 0.0f);
    qb_guiconstraint_width(text_box, QB_CONSTRAINT_PIXEL, 512.0f);
    qb_guiconstraint_height(text_box, QB_CONSTRAINT_PIXEL, 64.0f);
  }
  qb_guiblock_movetoback(parent);
  qb_guiblock_movetofront(text_box);

  qbGuiBlock main_menu;
  {
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.2f, 0.2f, 0.2f, 0.99f };
    attr.radius = 10.f;

    qbGuiBlockCallbacks_ callbacks = {};
    callbacks.onfocus = [](qbGuiBlock b) {
      std::cout << "ONFOCUS MAIN_MENU\n";
      return true;
    };
    callbacks.onmove = [](qbGuiBlock b, qbMouseMotionEvent e, int sx, int sy) {
      qb_guiblock_moveby(b, { (float)e->xrel, (float)e->yrel });
      return true;
    };

    attr.callbacks = &callbacks;

    qb_guiblock_create(&main_menu, &attr, "main_menu");
    qb_guiblock_resizeto(main_menu, { 250.f, 0.f });

    qb_guiconstraint_x(main_menu, QB_CONSTRAINT_MIN, 0.f);
    qb_guiconstraint_y(main_menu, QB_CONSTRAINT_MIN, 0.f);
    qb_guiconstraint_width(main_menu, QB_CONSTRAINT_MIN, 50.f);
    qb_guiconstraint_height(main_menu, QB_CONSTRAINT_MIN, 50.f);
    qb_guiconstraint_height(main_menu, QB_CONSTRAINT_RELATIVE, -150.f);
    
    windows.push_back(main_menu);
  }

  qbGuiBlock toolbar;
  {    
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.f, 0.f, 0.f, 0.f };

    qb_guiblock_create(&toolbar, &attr, "toolbar");
    qb_guiblock_link(main_menu, toolbar);
    qb_guiconstraint_x(toolbar, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_y(toolbar, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_width(toolbar, QB_CONSTRAINT_PERCENT, 1.f);
    qb_guiconstraint_height(toolbar, QB_CONSTRAINT_PIXEL, 30.f);

    windows.push_back(toolbar);
  }

  qbGuiBlock body;
  {    
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.f, 0.f, 0.f, 0.f };

    qb_guiblock_create(&body, &attr, "body");
    qb_guiblock_link(main_menu, body);
    qb_guiconstraint_x(body, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_y(body, QB_CONSTRAINT_RELATIVE, 30.f);
    qb_guiconstraint_width(body, QB_CONSTRAINT_PERCENT, 1.f);
    qb_guiconstraint_height(body, QB_CONSTRAINT_RELATIVE, -30.f);

    windows.push_back(body);
  }

  qbGuiBlock red_button;
  {
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 1.0f, 0.35f, 0.35f, 1.f };
    attr.radius = 2.f;

    qb_guiblock_create(&red_button, &attr, "close");
    qb_guiblock_link(toolbar, red_button);
    qb_guiconstraint_x(red_button, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_y(red_button, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_width(red_button, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guiconstraint_height(red_button, QB_CONSTRAINT_PIXEL, 10.f);

    windows.push_back(red_button);
  }

  qbGuiBlock yellow_button;
  {
    qbGuiBlockCallbacks_ callbacks = {};
    callbacks.onclick = [](qbGuiBlock w, qbMouseButtonEvent e) {
      const char* path[] = {"main_menu", "body", 0};
      qb_guiblock_closequery(path);

      const char* main_menu_path[] = { "main_menu", 0 };
      qbGuiBlock main_menu = qb_guiblock_find(main_menu_path);

      qb_guiconstraint_height(main_menu, QB_CONSTRAINT_MAX, 30.f);
      return true;
    };

    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 1.0f, 1.0f, 0.35f, 1.f };
    attr.radius = 2.f;
    attr.callbacks = &callbacks;

    qb_guiblock_create(&yellow_button, &attr, "minimize");
    qb_guiblock_link(toolbar, yellow_button);
    qb_guiconstraint_x(yellow_button, QB_CONSTRAINT_RELATIVE, 25.f);
    qb_guiconstraint_y(yellow_button, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_width(yellow_button, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guiconstraint_height(yellow_button, QB_CONSTRAINT_PIXEL, 10.f);

    windows.push_back(yellow_button);
  }

  qbGuiBlock green_button;
  {
    qbGuiBlockCallbacks_ callbacks = {};
    callbacks.onclick = [](qbGuiBlock w, qbMouseButtonEvent e) {
      const char* path[] = { "main_menu", "body", 0 };
      qb_guiblock_openquery(path);

      const char* main_menu_path[] = { "main_menu", 0 };
      qbGuiBlock main_menu = qb_guiblock_find(main_menu_path);

      qb_guiconstraint_clearheight(main_menu, QB_CONSTRAINT_MAX);
      return true;
    };

    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.35f, 1.0f, 0.35f, 1.f };
    attr.radius = 2.f;
    attr.callbacks = &callbacks;

    qb_guiblock_create(&green_button, &attr, "maximize");
    qb_guiblock_link(toolbar, green_button);
    qb_guiconstraint_x(green_button, QB_CONSTRAINT_RELATIVE, 40.f);
    qb_guiconstraint_y(green_button, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_width(green_button, QB_CONSTRAINT_PIXEL, 10.f);
    qb_guiconstraint_height(green_button, QB_CONSTRAINT_PIXEL, 10.f);

    windows.push_back(green_button);
  }

  qbGuiBlock display;
  {
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.35f, 0.35f, 0.35f, 1.f };
    attr.radius = 2.f;

    qb_guiblock_create(&display, &attr, "display");
    qb_guiblock_link(body, display);
    qb_guiconstraint_x(display, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_y(display, QB_CONSTRAINT_RELATIVE, 0.f);
    qb_guiconstraint_width(display, QB_CONSTRAINT_RELATIVE, -20.f);
    qb_guiconstraint_height(display, QB_CONSTRAINT_RELATIVE, -20.f);

    windows.push_back(display);
  }

  qbGuiBlock slider = qb_guislider_create({ 0.f, 500.f }, 200.f, "slider1");
  qb_guiblock_link(display, slider);  
  //qb_guiblock_link(display, qb_guislider_create({ 0.f, 300.f }, 200.f, "slider2"));

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

    qbGuiBlock new_game_text;
    qb_textbox_create(&new_game_text, &attr, "lorem_ipsum", { 800.0f, 64.0f }, 8, lorem_ipsum);

    qb_guiblock_link(display, new_game_text);
    qb_guiconstraint_x(new_game_text, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_y(new_game_text, QB_CONSTRAINT_RELATIVE, 10.f);
    qb_guiconstraint_width(new_game_text, QB_CONSTRAINT_RELATIVE, -20.f);

    windows.push_back(new_game_text);
  }
  
  qb_guiblock_close(main_menu);
  qb_guiblock_open(main_menu);

  qb_scene_attach(main_menu_scene, "main_menu", new std::vector<qbGuiBlock>(std::move(windows)));
  qb_coro_sync([](qbVar text) {
    qbGuiBlock text_box = (qbGuiBlock)text.p;
    while (true) {
      int fps = (int)timing_info.render_fps;
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
  }, qbPtr(text_box));

  {
    const int count = 9;
    const float size = 64.f;

    qbGuiBlock actionbar_container;
    {
      qbGuiBlockAttr_ attr = {};
      attr.background_color = { 0.f, 0.f, 0.f, 1.f };

      qb_guiblock_create(&actionbar_container, &attr, "actionbar_container");

      qb_guiconstraint(actionbar_container, QB_CONSTRAINT_PERCENT, QB_GUI_X, 0.5f);
      qb_guiconstraint(actionbar_container, QB_CONSTRAINT_RELATIVE, QB_GUI_X, 0.5f);
      qb_guiconstraint(actionbar_container, QB_CONSTRAINT_PERCENT, QB_GUI_Y, 0.f);

      qb_guiconstraint(actionbar_container, QB_CONSTRAINT_PERCENT, QB_GUI_WIDTH, 1.f);
      qb_guiconstraint(actionbar_container, QB_CONSTRAINT_PIXEL, QB_GUI_HEIGHT, size);
    }

    qbGuiBlock action_bar[count];
    qbGuiBlockAttr_ attr = {};
    attr.background_color = { 0.9f, 0.9f, 0.9f, 0.9f };
    attr.radius = 2.0f;
    attr.offset = { -size * 0.5f, -size * 0.5f };

    for (int i = 0; i < count; ++i) {
      qb_guiblock_create(&action_bar[i], &attr, (std::string("actionbar") + std::to_string(i)).data());

      qbGuiBlock b = action_bar[i];
      qb_guiconstraint(b, QB_CONSTRAINT_PIXEL, QB_GUI_X, size * i + 32.f);
      qb_guiconstraint(b, QB_CONSTRAINT_PIXEL, QB_GUI_Y, 500.f);

      qb_guiconstraint(b, QB_CONSTRAINT_PIXEL, QB_GUI_WIDTH, size);
      qb_guiconstraint(b, QB_CONSTRAINT_PIXEL, QB_GUI_HEIGHT, size);
    }
  }

  qb_guiblock_movetofront(slider);
  qb_scene_reset();
}

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
    qb_entity_create(&block, attr);
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
    qbCamera camera = qb_camera_active();
    int mouse_x, mouse_y;
    qb_mouse_position(&mouse_x, &mouse_y);
    float cam_dir = 0.0f, cam_zdir = 0.0f;
    float cam_dis = 20.0f;
    float cam_dis_dt = 0.0f;
    while (true) {
      t->orientation = glms_rotate(t->orientation, -0.001f, vec3s{ 0.0f, 0.0f, 1.0f });

      //t->position.z = 0.0f + 5.0f*cos((float)(frame) / 100.0f);
      //t->position.y = 0.0f - 5.0f*sin((float)(frame) / 100.0f);

      int x, y;
      qb_mouse_relposition(&x, &y);

      float dx, dy;
      dx = (float)x * -0.01f;
      dy = (float)y * 0.01f;

      float speed = 0.5f;
      if (qb_key_pressed(qbKey::QB_KEY_LSHIFT)) {
        speed *= 10.0f;
      }
      if (qb_key_pressed(qbKey::QB_KEY_LCTRL)) {
        speed *= 0.1f;
      }

      int wx, wy;
      qb_mouse_wheel(&wx, &wy);
      if (wx != 0 || wy != 0) {
        std::cout << wx << ", " << wy << std::endl;
      }

      if (qb_mouse_relative()) {
        mat4s m = camera->rotation_mat;
        m = glms_rotate(m, dx, vec3s{ 0, 0, 1 });
        m = glms_rotate(m, -dy, vec3s{ 0, 1, 0 });
        qb_camera_rotation(camera, m);

        /*cam_dis += cam_dis_dt;

        vec4s orig = glms_mat4_mulv(m, vec4s{ -cam_dis, 0.f, 0.f, 1.0f });
        qb_camera_origin(camera, { orig.x, orig.y, orig.z });
        cam_dis_dt *= 0.9f;*/
      }

      static uint32_t was_pressed = 0;
      if (qb_key_pressed(QB_KEY_F11) && !was_pressed) {
        was_pressed = 1;
        if (qb_window_fullscreen() == QB_WINDOWED) {
          qb_window_setfullscreen(QB_WINDOW_FULLSCREEN_DESKTOP);
        } else {
          qb_window_setfullscreen(QB_WINDOWED);
        }
      } else if (!qb_key_pressed(QB_KEY_F11)) {
        was_pressed = 0;
      }

      if (qb_key_pressed(qbKey::QB_KEY_W)) {
        /*if (qb_key_pressed(qbKey::QB_KEY_LSHIFT)) {
          cam_dis_dt = -0.1f;
        } else {
          cam_dis_dt = -0.01f;
        }*/
        vec4s dir = { -speed, 0.0f, 0.0f, 1.0f };
        qb_camera_origin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
      }

      if (qb_key_pressed(qbKey::QB_KEY_S)) {
        /*if (qb_key_pressed(qbKey::QB_KEY_LSHIFT)) {
          cam_dis_dt = 0.1f;
        } else {
          cam_dis_dt = 0.01f;
        }*/
        
        vec4s dir = { speed, 0.0f, 0.0f, 1.0f };
        qb_camera_origin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
          
      }

      if (qb_key_pressed(qbKey::QB_KEY_A)) {
        vec4s dir = { 0.0f, -speed, 0.0f, 1.0f };
        qb_camera_origin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
      }

      if (qb_key_pressed(qbKey::QB_KEY_D)) {
        vec4s dir = { 0.0f, speed, 0.0f, 1.0f };
        qb_camera_origin(
          camera,
          glms_vec3_add(camera->origin, glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir))));
      }

      if (qb_key_pressed(qbKey::QB_KEY_Q)) {
        qb_camera_rotation(camera, glms_rotate(camera->rotation_mat, 0.025f, vec3s{ 1, 0, 0 }));
      }

      if (qb_key_pressed(qbKey::QB_KEY_E)) {
        qb_camera_rotation(camera, glms_rotate(camera->rotation_mat,  -0.025f, vec3s{ 1, 0, 0 }));
      }

      /*

      int x, y;
      qb_mouse_relposition(&x, &y);

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
        qb_camera_rotation(camera, m);
      }
      
      vec4s dir = {};
      if (qb_key_pressed(qbKey::QB_KEY_W)) {
        dir = { 0.5f, 0.0f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_origin(camera, glms_vec3_add(camera->origin, d));
      }

      if (qb_key_pressed(qbKey::QB_KEY_S)) {
        dir = { -0.5f, 0.0f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_origin(camera, glms_vec3_add(camera->origin, d));
      }

      if (qb_key_pressed(qbKey::QB_KEY_A)) {
        dir = { 0.0f, 0.5f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_origin(camera, glms_vec3_add(camera->origin, d));
      }

      if (qb_key_pressed(qbKey::QB_KEY_D)) {
        dir = { 0.0f, -0.5f, 0.0f, 1.0f };
        vec3s d = glms_vec3(glms_mat4_mulv(camera->rotation_mat, dir));
        d.z = 0.0f;
        qb_camera_origin(camera, glms_vec3_add(camera->origin, d));
      }*/

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
    qbTransform a_t, b_t, c_t;
    qb_instance_find(qb_transform(), a_block, &a_t);
    qb_instance_find(qb_transform(), b_block, &b_t);
    qb_instance_find(qb_transform(), c_block, &c_t);

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
    qb_mouse_position(&mouse_x, &mouse_y);
    vec3s mouse_dir = qb_camera_screentoworld(qb_camera_active(), { (float)mouse_x, (float)mouse_y });
    qbRay_ mouse_ray{ qb_camera_active()->origin, mouse_dir };

    c_t->position = glms_vec3_add(qb_camera_active()->origin, glms_vec3_scale(mouse_dir, dis));

    int scroll_x, scroll_y;
    qb_mouse_wheel(&scroll_x, &scroll_y);
    dis += (float)scroll_y;

    if (qb_collider_checkray(a, a_t, &mouse_ray)) {
      (*material)->albedo = { 1.f, 0.0f, 0.f };
    } else {
      (*material)->albedo = { 0.f, 1.f, 0.f };
    }

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

  qb_mouse_setrelative(0);

  qb_coro_sync(test_collision, qbNone);

  qbLoopCallbacks_ loop_callbacks = {};
  qbLoopArgs_ loop_args = {};  
  while (qb_loop(&loop_callbacks, &loop_args) != QB_DONE) {
    qb_timing(uni, &timing_info);
  }
  return 0;
}