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
    qb_shader_load(&mesh_shader, "mesh_shader", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\mesh.vs", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\mesh.fs");
    qb_texture_load(&ball_texture, "ball_texture", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\ball.bmp");
    qb_mesh_load(&block_mesh, "block_mesh", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\block.obj");
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
                 {0.0f, 0.0f, 0.0f}, true);
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
    qb_mesh_load(&mesh, "floor_mesh", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\floor.obj");
    render::qbRenderable renderable = render::create(mesh, ball_material);
    //qb_entityattr_addcomponent(attr, render::component(), &renderable);

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
  {
    qbMesh terrain;
    MeshBuilder builder;
    const size_t width = 50;
    const size_t height = 50;
    const float scale = 50.0f;

    auto height_map = std::make_unique<std::vector<std::vector<float>>>();
    height_map->reserve(height);
    for (size_t y = 0; y < height; ++y) {
      height_map->push_back(std::vector<float>(width, 0.0f));
      for (size_t x = 0; x < width; ++x) {
        (*height_map)[y][x] = 0;
      }
    }

    std::vector<physics::Transform> mountain_generators(10);
    for (physics::Transform& t : mountain_generators) {
      t.p = { width / 2, height / 2, 1.0f };
      t.v = glm::rotate(glm::quat({ 0, 0, glm::radians((float)(rand() % 360)) }), glm::vec3{ 1.0f, 0.0, -0.001 });
    }

    const float branching_prob = 0.005f;
    const int num_branches = 3;
    const float max_distance = width / 2;

    while(!mountain_generators.empty()) {
      auto it = mountain_generators.begin();
      physics::Transform& t = *it;

      std::vector<physics::Transform> new_branches;
      while (t.p.x >= 0 && t.p.y >= 0 && t.p.x < width && t.p.y < height && t.p.z > 0.0f) {
        if ((float)(rand() % 100000) / 100000.0f < branching_prob) {
          physics::Transform original = t;
          int branches = rand() % num_branches + 1;
          for (int i = 0; i < branches; ++i) {
            physics::Transform new_branch = original;
            new_branch.v = glm::rotate(glm::quat({ 0, 0, glm::radians(((float)(rand() % 1000) / 1000.0f) * 20 - 10.0f) }), original.v);
            new_branches.push_back(std::move(new_branch));
          }
          break;
        }
        t.v = glm::rotate(glm::quat({ 0, 0, glm::radians(((float)(rand() % 1000) / 1000.0f) - 0.5f) }), t.v);
        (*height_map)[(int)t.p.y][(int)t.p.x] += t.p.z;
        t.p += t.v;
      }
      mountain_generators.erase(it);
      for (auto& branch : new_branches) {
        mountain_generators.push_back(std::move(branch));
      }
    }

    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        builder.add_vertex(glm::vec3{ x, y, (*height_map)[y][x] } * scale);
      }
    }

    builder.add_texture({ 0, 0 });
    builder.add_texture({ 1, 0 });
    builder.add_texture({ 1, 1 });
    builder.add_texture({ 0, 1 });

    builder.add_normal({ 0, 0, 1 });

    auto coord_to_index = [height](size_t x, size_t y) {
      return y * height + x;
    };
    for (size_t y = 0; y < height - 1; ++y) {
      for (size_t x = 0; x < width - 1; ++x) {
        {
          MeshBuilder::Face face;
          face.v[0] = coord_to_index(x, y);
          face.v[1] = coord_to_index(x + 1, y);
          face.v[2] = coord_to_index(x + 1, y + 1);
          face.vn[0] = face.vn[1] = face.vn[2] = 0;

          face.vt[0] = 0;
          face.vt[1] = 1;
          face.vt[2] = 2;

          builder.add_face(std::move(face));
        }
        {
          MeshBuilder::Face face;
          face.v[0] = coord_to_index(x + 1, y + 1);
          face.v[1] = coord_to_index(x, y + 1);
          face.v[2] = coord_to_index(x, y);
          face.vn[0] = face.vn[1] = face.vn[2] = 0;

          face.vt[0] = 2;
          face.vt[1] = 3;
          face.vt[2] = 0;

          builder.add_face(std::move(face));
        }
      }
    }

#if 0
    for (size_t y = 0; y < height - 1; ++y) {
      for (size_t x = 0; x < width - 1; ++x) {
        glm::vec3 tl = glm::vec3{ x, y, 0 } *scale;
        glm::vec3 tr = glm::vec3{ x + 1, y, 0 } *scale;
        glm::vec3 br = glm::vec3{ x + 1, y + 1, 0 } *scale;
        glm::vec3 bl = glm::vec3{ x, y + 1, 10 } *scale;

        builder.add_face(
        { tl, tr, br },
        { { 0, 0 },{ 1, 0 },{ 1, 1 } },
        { { 0, 0, 1 },{ 0, 0, 1 },{ 0, 0, 1 } });

        builder.add_face(
        { br, bl, tl },
        { { 1, 1 },{ 0, 1 },{ 0, 0 } },
        { { 0, 0, 1 },{ 0, 0, 1 },{ 0, 0, 1 } });
      }
    }
#endif
    terrain = builder.build();

    qbEntityAttr e_attr;
    qb_entityattr_create(&e_attr);

    physics::Transform t{ { 0.0f, 0.0f, -32.0f }, {}, true };
    qb_entityattr_addcomponent(e_attr, physics::component(), &t);

    render::qbRenderable renderable = render::create(terrain, ball_material);
    qb_entityattr_addcomponent(e_attr, render::component(), &renderable);

    qbEntity entity;
    qb_entity_create(&entity, e_attr);

    qb_entityattr_destroy(&e_attr);
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
    if (rand() % 10 == 0) {
      ball::create({(float)(rand() % 500) - 250.0f,
                    (float)(rand() % 500) - 250.0f,
                    (float)(rand() % 500) - 250.0f}, {}, true, true);
    }
      

    if (trigger % period == 0 && prev_trigger != trigger) {
    //if (true && period && prev_trigger == prev_trigger && trigger == trigger) {
      std::cout << "Ball count " << qb_component_getcount(ball::Component()) << std::endl;
      double total = 15 * 1e6;
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
