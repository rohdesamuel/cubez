#include <cubez/cubez.h>
#include <cubez/timer.h>

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
    qb_scene_create(&main_menu);
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

int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);

  qb_start();
  int frame = 0;
  WindowTimer fps_timer(50);
  WindowTimer update_timer(50);
  WindowTimer render_timer(50);

  double t = 0.0;
  const double dt = 0.01;
  double current_time = Timer::now() * 0.000000001;
  double start_time = Timer::now();
  double accumulator = 0.0;
  gui::qbRenderTarget target;
  gui::qb_rendertarget_create(&target, { 250, 0 }, { 500, 500 });

  qb_loop();
  while (1) {
    fps_timer.start();

    double new_time = Timer::now() * 0.000000001;
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
      update_timer.start();
      qb_loop();
      update_timer.stop();
      update_timer.step();

      accumulator -= dt;
      t += dt;
    }

    render_timer.start();

    render::RenderEvent e;
    e.frame = frame;
    e.ftimestamp_us = Timer::now() - start_time;
    e.alpha = accumulator / dt;
    render::present(&e);
    
    render_timer.stop();
    render_timer.step();

    ++frame;
    fps_timer.stop();
    fps_timer.step();

    double time = Timer::now();

    static int prev_trigger = 0;
    static int trigger = 0;
    int period = 1;

    prev_trigger = trigger;
    trigger = int64_t(time - start_time) / 1000000000;
    if (rand() % 1000000000 == 0) {
      ball::create({(float)(rand() % 500) - 250.0f,
                    (float)(rand() % 500) - 250.0f,
                    (float)(rand() % 500) - 250.0f}, {}, true, true);
    }
      

    if (trigger % period == 0 && prev_trigger != trigger) {
      if ((int)(1e9 / update_timer.get_avg_elapsed_ns()) < 60) {
        std::cout << "BAD FPS\n";
      }
    //if (true && period && prev_trigger == prev_trigger && trigger == trigger) {
      std::cout << "Ball count " << qb_component_getcount(ball::Component()) << std::endl;
      double total = (1/60.0) * 1e9;
      //logging::out(
      std::cout <<
          "Frame " + std::to_string(frame) + "\n" +
          + "Utili: "  + std::to_string(100.0 * render_timer.get_avg_elapsed_ns() / total)
          + " : " + std::to_string(100.0 * update_timer.get_avg_elapsed_ns() / total) + "\n"
          + "Render FPS: " + std::to_string((int)(1e9 / render_timer.get_avg_elapsed_ns())) + "\n"
          + "Update FPS: " + std::to_string((int)(1e9 / update_timer.get_avg_elapsed_ns())) + "\n"
          + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n"
          + "Accum: " + std::to_string(accumulator) + "\n"
          + "Alpha: " + std::to_string(accumulator / dt) + "\n";
    }
  }
}
