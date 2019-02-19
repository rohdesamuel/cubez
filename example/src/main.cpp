#include <cubez/cubez.h>
#include <cubez/utils.h>

#include "ball.h"
#include "physics.h"
#include "player.h"
#include "log.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "input.h"
#include "render.h"
#include "shader.h"
#include "planet.h"
#include "gui.h"

#include <algorithm>
#include <thread>
#include <unordered_map>

const char simple_vs[] = R"(
#version 200

in vec3 inPos;
in vec3 inCol;

out vec3 vCol;

void main() {
  vCol = inCol;
  gl_Position = vec4(inPos, 1.0);
}
)";

const char simple_fs[] = R"(
#version 130

in vec3 vCol;
out vec4 frag_color;

void main() {
  frag_color = vec4(vCol, 1.0);
}
)";


void check_for_gl_errors() {
  GLenum error = glGetError();
  if (error) {
    const GLubyte* error_str = gluErrorString(error);
    std::cout << "Error(" << error << "): " << error_str << std::endl;
  }
}

void initialize_universe(qbUniverse* uni) {
  qb_init(uni);

  {
    logging::initialize();
  }

  {
    physics::Settings settings;
    physics::initialize(settings);
  }
  
  {
    render::Settings s;
    s.title = "Cubez example";
    s.width = 1200;
    s.height = 800;
    s.znear = 0.1f;
    s.zfar = 10000.0f;
    s.fov = 70.0f;
    render::initialize(s);

    render::qb_camera_setyaw(90.0f);
    render::qb_camera_setpitch(0.0f);
    render::qb_camera_setposition({400.0f, -250.0f, 250.0f});

    check_for_gl_errors();
  }
  
  {
    input::initialize();
  }
  {
    /*

    // Operations done in the global scene (QB_GLOBAL_SCENE) will persist until
    // the game ends. By default, all API calls are executed on the global
    // scene.
    qb_scene_activate(QB_GLOBAL_SCENE);
    qb_texture_load(...);
    qb_model_load(...);

    // Create two scenes: one for the main menu, and then another scene that
    // holds all the game objects.
    qbScene main_menu;
    qbScene game_prefetch;
    qb_scene_load(&main_menu);
    qb_scene_create(&game_prefetch);

    // Sets the scene to be active. While the scene is active, all API calls
    // will only affect the active scene. The game loop will only run over
    // the active scene.
    qb_scene_activate(main_menu);
    qb_component_create(...);
    qb_entity_create(...);
    qb_material_create(...);
    qb_event_send(...);
    

    // "set"ing a scene will not run the game loop over it. All API calls
    // performed will only be executed on the "set" scene. The game loop will
    // NOT run over the "set" scene.
    qb_with_scene(game_prefetch, [](){
      qb_scene_push(game_prefetch);
      qb_entity_create(...);
      qb_model_load(...);
      qb_scene_pop();
    });

    ...

    // Once the user clicks on the "Start Game" button, activate the game scene
    // and purge the old scene.
    qb_button_press("MainMenu/StartGame", []() {
      qb_scene_activate(game_prefetch);
      qb_scene_destroy(main_menu);
    });

    */
  }

#if 1
  qbShader mesh_shader;
  qbTexture ball_texture;
  MeshBuilder block_mesh;
  qbMaterial ball_material;
  qbMaterial red_material;
  {
    // Load resources.
    std::cout << "Loading resources.\n";
    qb_shader_load(&mesh_shader, "mesh_shader", "resources/mesh.vs", "resources/mesh.fs");
    qb_texture_load(&ball_texture, "ball_texture", "resources/soccer_ball.bmp");
    block_mesh = MeshBuilder::FromFile("resources/block.obj");

    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setcolor(attr, glm::vec4{ 1.0, 0.0, 1.0, 1.0 });
    qb_materialattr_addtexture(attr, ball_texture, {0, 0}, {0, 0});
    qb_materialattr_setshader(attr, mesh_shader);
    qb_material_create(&ball_material, attr);
    qb_materialattr_destroy(&attr);
  }
  {
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setcolor(attr, glm::vec4{ 1.0, 0.0, 0.0, 1.0 });
    qb_materialattr_setshader(attr, mesh_shader);
    qb_material_create(&red_material, attr);
    qb_materialattr_destroy(&attr);
  }

  {
    ball::Settings settings;
    settings.mesh = block_mesh.BuildRenderable(qbRenderMode::QB_TRIANGLES);
    settings.collision_mesh = new Mesh(block_mesh.BuildMesh());
    settings.material = ball_material;
    settings.material_exploded = red_material;

    ball::initialize(settings);
    check_for_gl_errors();
  }
#if 0
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    physics::Transform t{{400.0f, 300.0f, -32.0f}, {}, true};
    qb_entityattr_addcomponent(attr, physics::component(), &t);

    qbMesh mesh;
    qb_mesh_load(&mesh, "floor_mesh", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\floor.obj");
    render::create(mesh, ball_material);

    qbEntity unused;
    qb_entity_create(&unused, attr);

    qb_entityattr_destroy(&attr);
  }
#endif
  {
    player::Settings settings;
    settings.start_pos = {0, 0, 0};

    player::initialize(settings);
    check_for_gl_errors();
  }

  /*
  
  qbScene main_menu_scene;

  qb_scene_create(&main_menu_scene);
  qb_scene_push(main_menu_scene);

  

  qb_scene_pop();

  */

  auto new_game = [mesh_shader](const framework::JSObject&, const framework::JSArgs&) {
    planet::Settings settings;
    settings.shader = mesh_shader;
    settings.resource_folder = "resources/";
    planet::Initialize(settings);

    return framework::JSValue();
  };
  {
    planet::Settings settings;
    settings.shader = mesh_shader;
    settings.resource_folder = "resources/";
    planet::Initialize(settings);
  }
  {
    glm::vec2 menu_size(render::window_width(), render::window_height());
    glm::vec2 menu_pos(0, 0);

    gui::JSCallbackMap callbacks;
    callbacks["NewGame"] = new_game;

    //gui::FromFile("file:///game.html", menu_pos, menu_size, callbacks);
  }

#endif
}

qbCoro test_coro;
qbCoro generator_coro;
qbCoro wait_coro;

qbVar load_texture(qbVar var) {
  qbTexture tex = nullptr;
  qb_texture_load(&tex, "", (const char*)var.p);
  
  return qbVoid(tex);
}

qbVar generator(qbVar var) {
  qbCoro c = qb_coro_async(load_texture, qbVoid("resources/soccer_ball.bmp"));

  for (;;) {
    var = qb_coro_yield(qbInt(var.i - 1));    

    qbVar ret = qb_coro_await(c);
    if ((ret = qb_coro_peek(c)).tag != QB_TAG_UNSET) {
      logging::out("Texture loaded!");
      var = qbInt(0);
      qb_coro_destroy(&c);
    }
    //qb_coro_wait(1.0);
  }
  return qbNone;
}

qbVar test_entry(qbVar var) {
  qbVar ret = var;
  int num_times_run = 0;

  while (true) {
    if (ret.tag != QB_TAG_UNSET) {
      logging::out("%lld", ret.i);
    }
    ret = qb_coro_call(generator_coro, ret);
    //qb_coro_await(generator_coro);
    num_times_run++;

    qb_coro_yield(qbNone);
    if (ret.tag == QB_TAG_UNSET) {
      continue;
    } else if (ret.i <= 0) {
      break;
    }
  }

  logging::out("number of times run: %d", num_times_run);
  return qbNone;
}

struct TimingInfo {
  uint64_t frame;
  float udpate_fps;
  float render_fps;
  float total_fps;
};

qbVar print_timing_info(qbVar var) {
  for (;;) {
    TimingInfo* info = (TimingInfo*)var.p;

    std::string out =
      "Frame: " + std::to_string(info->frame) + "\n"
      "Total FPS:  " + std::to_string(info->total_fps) + "\n"
      "Render FPS: " + std::to_string(info->render_fps) + "\n"
      "Update FPS: " + std::to_string(info->udpate_fps);

    logging::out(out.c_str());

    qb_coro_wait(1.0);
  }
}

qbVar create_random_balls(qbVar) {
  for (;;) {
    glm::vec3 v = {
      (((float)(rand() % 1000)) / 1000) - 0.5,
      (((float)(rand() % 1000)) / 1000) - 0.5,
      (((float)(rand() % 1000)) / 1000) - 0.5
    };
    qbEntity ball = ball::create({}, v, true);
    qb_coro_wait(10);
    qb_entity_destroy(ball);
  }
}

int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);

  qb_coro_sync(create_random_balls, qbNone);
  test_coro = qb_coro_sync(test_entry, qbInt(100000));
  generator_coro = qb_coro_create(generator);

  //qb_coro_call(test_coro, qbInt(100000));
  
  qb_start();
  TimingInfo timing_info;
  int frame = 0;
  qbTimer fps_timer;
  qbTimer update_timer;
  qbTimer render_timer;

  qb_coro_sync(print_timing_info, qbVoid(&timing_info));

  qb_timer_create(&fps_timer, 50);
  qb_timer_create(&update_timer, 50);
  qb_timer_create(&render_timer, 50);

  const uint64_t kClockResolution = 1e9;
  double t = 0.0;
  const double dt = 0.01;
  double current_time = qb_timer_query() * 0.000000001;
  double start_time = qb_timer_query();
  double accumulator = 0.0;
  gui::qbRenderTarget target;
  //gui::qb_rendertarget_create(&target, { 250, 0 }, { 500, 500 }, {});

  qb_loop();
  while (1) {
    qb_timer_start(fps_timer);

    double new_time = qb_timer_query() * 0.000000001;
    double frame_time = new_time - current_time;
    frame_time = std::min(0.25, frame_time);
    current_time = new_time;

    accumulator += frame_time;
    
    input::handle_input([](SDL_Event*) {
      render::shutdown();
      SDL_Quit();
      qb_stop();
      exit(0);
    });
    while (accumulator >= dt) {
      qb_timer_start(update_timer);
      qb_loop();
      qb_timer_add(update_timer);

      accumulator -= dt;
      t += dt;
    }

    qb_timer_start(render_timer);

    render::RenderEvent e;
    e.frame = frame;
    e.ftimestamp_us = qb_timer_query() - start_time;
    e.alpha = accumulator / dt;
    render::present(&e);
    
    qb_timer_add(render_timer);

    ++frame;
    qb_timer_stop(fps_timer);

    // Calculate then communicate TimingInfo to Coroutine.
    auto update_timer_avg = qb_timer_average(update_timer);
    auto render_timer_avg = qb_timer_average(render_timer);
    auto fps_timer_elapsed = qb_timer_elapsed(fps_timer);

    int update_fps = update_timer_avg == 0 ? 0 : kClockResolution / update_timer_avg;
    int render_fps = render_timer_avg == 0 ? 0 : kClockResolution / render_timer_avg;
    int total_fps = fps_timer_elapsed == 0 ? 0 : kClockResolution / fps_timer_elapsed;

    timing_info.frame = frame;
    timing_info.render_fps = render_fps;
    timing_info.total_fps = total_fps;
    timing_info.udpate_fps = update_fps;
  }
}
