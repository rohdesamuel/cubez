#include "inc/cubez.h"
#include "inc/timer.h"

#include "ball.h"
#include "physics.h"
#include "player.h"
#include "log.h"
#include "mesh.h"
#include "input.h"
#include "render.h"
#include "shader.h"

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
    s.width = 800;
    s.height = 600;
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

  qbShader mesh_shader;
  qbTexture ball_texture;
  qbMesh block_mesh;
  qbMaterial ball_material;
  {
    // Load resources.
    std::cout << "Loading resources.\n";
    qb_shader_load(&mesh_shader, "mesh_shader", "mesh.vs", "mesh.fs");
    qb_texture_load(&ball_texture, "ball_texture", "ball.bmp");
    qb_mesh_load(&block_mesh, "block_mesh", "block.obj");
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);

    qb_materialattr_addtexture(attr, ball_texture, {0, 0}, {0, 0});
    qb_materialattr_setshader(attr, mesh_shader);
    qb_material_create(&ball_material, attr);

    qb_materialattr_destroy(&attr);
  }

  {
    ball::Settings settings;
    settings.mesh = block_mesh;
    settings.material = ball_material;

    ball::initialize(settings);

    ball::create({0.0f, 0.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({800.0f, 0.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({0.0f, 600.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({800.0f, 600.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({0.0f, 0.0f, 100.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({800.0f, 0.0f, 100.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({0.0f, 600.0f, 100.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({800.0f, 600.0f, 100.0f},
                 {0.0f, 0.0f, 0.0f});

    check_for_gl_errors();
  }
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    physics::Transform t{{400.0f, 300.0f, -32.0f}, {}, true};
    qb_entityattr_addcomponent(attr, physics::component(), &t);

    qbMesh mesh;
    qb_mesh_load(&mesh, "floor_mesh", "floor.obj");
    render::qbRenderable renderable = render::create(mesh, ball_material);
    qb_entityattr_addcomponent(attr, render::component(), &renderable);

    qbEntity unused;
    qb_entity_create(&unused, attr);

    qb_entityattr_destroy(&attr);
  }
  {
    player::Settings settings;
    settings.start_pos = {0, 0, 0};

    player::initialize(settings);
    check_for_gl_errors();
  }
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

  glViewport(0, 0, 800, 600);

  double t = 0.0;
  const double dt = 0.01;
  double current_time = Timer::now() * 0.000000001;
  double start_time = Timer::now();
  double accumulator = 0.0;

  qb_loop();
  while (1) {
    fps_timer.start();

    double new_time = Timer::now() * 0.000000001;
    double frame_time = new_time - current_time;
    frame_time = std::min(0.25, frame_time);
    current_time = new_time;

    accumulator += frame_time;

    while (accumulator >= dt) {
      input::handle_input([](SDL_Event*) {
          render::shutdown();
          SDL_Quit();
          qb_stop();
          exit(0);
        });
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
    if (rand() % 100000 == 0) {
      ball::create({(float)(rand() % 500) - 250.0f,
                    (float)(rand() % 500) - 250.0f,
                    (float)(rand() % 500) - 250.0f}, {});
    }
      

    if (trigger % period == 0 && prev_trigger != trigger) {
    //if (true && period && prev_trigger == prev_trigger && trigger == trigger) {
      std::cout << "Ball count " << qb_component_getcount(ball::Component()) << std::endl;
      double total = 15 * 1e6;
      logging::out(
          "Frame " + std::to_string(frame) + "\n" +
          + "Utili: "  + std::to_string(100.0 * render_timer.get_avg_elapsed_ns() / total)
          + " : " + std::to_string(100.0 * update_timer.get_avg_elapsed_ns() / total) + "\n"
          + "Render FPS: " + std::to_string((int)(1e9 / render_timer.get_avg_elapsed_ns())) + "\n"
          + "Update FPS: " + std::to_string((int)(1e9 / update_timer.get_avg_elapsed_ns())) + "\n"
          + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n"
          + "Accum: " + std::to_string(accumulator) + "\n");
    }
  }
}
