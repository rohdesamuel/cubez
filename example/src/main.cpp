#include <cubez/cubez.h>
#include <cubez/utils.h>
#include <cubez/log.h>
#include <cubez/mesh.h>
#include <cubez/input.h>
#include <cubez/render.h>
#include <cubez/audio.h>
#include <cubez/render_pipeline.h>
#include <cubez/gui.h>
#include <cubez/async.h>
#include <cubez/random.h>
#include <cubez/socket.h>
#include <cubez/struct.h>
#include <cubez/sprite.h>
#include <cubez/draw.h>
#include <cubez/nuklear.h>

#include "renderer.h"
#include "forward_renderer.h"
#include "renderer.h"
#include "pong.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cglm/struct.h>
#include <queue>

#define NOMINMAX
#include <winsock.h>

#include "earthgen/planet/planet.h"
#include "earthgen/planet/climate/climate.h"
#include "earthgen/planet/terrain/terrain.h"
#include "earthgen/planet/terrain/terrain_generation.h"
#include "earthgen/render/planet_colours.h"

void initialize_universe(qbUniverse* uni) {
  uint32_t width = 1200;
  uint32_t height = 800;

  qbUniverseAttr_ uni_attr = {};
  uni_attr.title = "Cubez example";
  uni_attr.width = width;
  uni_attr.height = height;

  qbRendererAttr_ renderer_attr = {};
  renderer_attr.create_renderer = qb_defaultrenderer_create;
  renderer_attr.destroy_renderer = qb_defaultrenderer_destroy;
  uni_attr.renderer_args = &renderer_attr;

  qbAudioAttr_ audio_attr = {};
  audio_attr.sample_frequency = 44100;
  audio_attr.buffered_samples = 8192;
  uni_attr.audio_args = &audio_attr;

  qbScriptAttr_ script_attr = {};
  script_attr.entrypoint = "main.lua";
  uni_attr.script_args = &script_attr;

  qbResourceAttr_ resource_attr = {};
  resource_attr.scripts = "scripts";
  resource_attr.fonts = "fonts";
  resource_attr.meshes = "models";
  uni_attr.resource_args = &resource_attr;

  qb_init(uni, &uni_attr);
}

const float PADDLE_WIDTH = 50.f;
const float PADDLE_HEIGHT = 200.f;
const float BALL_RADIUS = 50.f;

float paddle_center;
vec2s paddle_left{};
vec2s paddle_right{};

float rot = 0.f;
float rot_speed = 0.f;
float zrot = 0.f;
float zrot_speed = 0.f;


earthgen::Planet* selected_planet = nullptr;
const earthgen::Tile* selected_tile = nullptr;
qbCamera active_camera;

class PlanetMeshBuilder {
public:
  PlanetMeshBuilder(earthgen::Planet* planet): planet_(planet) {}

  qbMesh planet_mesh() {
    qbMeshBuilder b;
    qbMesh ret;
    qb_meshbuilder_create(&b);

    for (auto& t : earthgen::tiles(*planet_)) {
      add_tile(b, t);
    }

    qb_meshbuilder_build(b, QB_DRAW_MODE_TRIANGLES, &ret, nullptr);
    qb_meshbuilder_destroy(&b);

    return ret;
  }

  qbMesh tile_mesh(const earthgen::Tile& tile) {
    auto& terrain_tile = earthgen::terrain(*planet_).tiles[earthgen::id(tile)];
    auto& corners = earthgen::corners(tile);
    qbMeshBuilder b;
    qbMesh ret;
    qb_meshbuilder_create(&b);
    add_tile(b, tile);
    qb_meshbuilder_build(b, QB_DRAW_MODE_TRIANGLES, &ret, nullptr);
    qb_meshbuilder_destroy(&b);
    return ret;
  }

  void add_tile(qbMeshBuilder b, const earthgen::Tile& tile) {
    auto& terrain_tile = earthgen::terrain(*planet_).tiles[earthgen::id(tile)];
    auto& corners = earthgen::corners(tile);
    if (corners.size() == 5) {
      for (auto& face : pentagon_faces()) {
        int v[] = {
          qb_meshbuilder_addv(b, {corners[face[0]]->v.x, corners[face[0]]->v.y, corners[face[0]]->v.z}),
          qb_meshbuilder_addv(b, {corners[face[1]]->v.x, corners[face[1]]->v.y, corners[face[1]]->v.z}),
          qb_meshbuilder_addv(b, {corners[face[2]]->v.x, corners[face[2]]->v.y, corners[face[2]]->v.z})
        };

        int vn[] = {
          qb_meshbuilder_addvn(b, glms_vec3_normalize({corners[face[0]]->v.x, corners[face[0]]->v.y, corners[face[0]]->v.z})),
          qb_meshbuilder_addvn(b, glms_vec3_normalize({corners[face[1]]->v.x, corners[face[1]]->v.y, corners[face[1]]->v.z})),
          qb_meshbuilder_addvn(b, glms_vec3_normalize({corners[face[2]]->v.x, corners[face[2]]->v.y, corners[face[2]]->v.z}))
        };

        int vt[] = {
          qb_meshbuilder_addvt(b, pentagon_tex_coords()[terrain_tile.type][face[0]]),
          qb_meshbuilder_addvt(b, pentagon_tex_coords()[terrain_tile.type][face[1]]),
          qb_meshbuilder_addvt(b, pentagon_tex_coords()[terrain_tile.type][face[2]]),
        };

        qb_meshbuilder_addtri(b, v, vn, vt);
      }
    } else {
      for (auto& face : hexagon_faces()) {
        int v[] = {
          qb_meshbuilder_addv(b, {corners[face[0]]->v.x, corners[face[0]]->v.y, corners[face[0]]->v.z}),
          qb_meshbuilder_addv(b, {corners[face[1]]->v.x, corners[face[1]]->v.y, corners[face[1]]->v.z}),
          qb_meshbuilder_addv(b, {corners[face[2]]->v.x, corners[face[2]]->v.y, corners[face[2]]->v.z})
        };

        int vn[] = {
          qb_meshbuilder_addvn(b, glms_vec3_normalize({corners[face[0]]->v.x, corners[face[0]]->v.y, corners[face[0]]->v.z})),
          qb_meshbuilder_addvn(b, glms_vec3_normalize({corners[face[1]]->v.x, corners[face[1]]->v.y, corners[face[1]]->v.z})),
          qb_meshbuilder_addvn(b, glms_vec3_normalize({corners[face[2]]->v.x, corners[face[2]]->v.y, corners[face[2]]->v.z}))
        };

        int vt[] = {
          qb_meshbuilder_addvt(b, hexagon_tex_coords()[terrain_tile.type][face[0]]),
          qb_meshbuilder_addvt(b, hexagon_tex_coords()[terrain_tile.type][face[1]]),
          qb_meshbuilder_addvt(b, hexagon_tex_coords()[terrain_tile.type][face[2]]),
        };
        qb_meshbuilder_addtri(b, v, vn, vt);
      }
    }
  }

  static std::array<std::array<int, 3>, 3>& pentagon_faces() {
    static std::array<std::array<int, 3>, 3> faces = {
      std::array<int, 3>{0, 1, 4},
      std::array<int, 3>{1, 3, 4},
      std::array<int, 3>{1, 2, 3}
    };

    return faces;
  }

  static std::array<std::array<int, 3>, 4>& hexagon_faces() {
    static std::array<std::array<int, 3>, 4> faces = {
      std::array<int, 3>{0, 4, 5},
      std::array<int, 3>{0, 3, 4},
      std::array<int, 3>{0, 1, 3},
      std::array<int, 3>{1, 2, 3}
    };

    return faces;
  }

private:
  vec2s pentagon_uv(int tile_type, int corner_index) {
    vec2s center;
    if (tile_type & earthgen::Terrain_tile::type_coast) {
      center = vec2s{ 0.75f, 0.75f };
    } else if (tile_type & earthgen::Terrain_tile::type_land) {
      center = vec2s{ 0.25f, 0.25f };
    } else if (tile_type & earthgen::Terrain_tile::type_water) {
      center = vec2s{ 0.75f, 0.25f };
    } else {
      center = vec2s{ 0.5f, 0.5f };
    }

    vec2s ret{ 0.25f * cos((corner_index / 5.f) * 2.f * 3.141593654f), 0.25f * sin((corner_index / 5.f) * 2.f * 3.141593654f) };
    return glms_vec2_add(ret, center);
  }

  vec2s hexagon_uv(int tile_type, int corner_index) {
    vec2s center;
    if (tile_type & earthgen::Terrain_tile::type_coast) {
      center = vec2s{ 0.75f, 0.75f };
    } else if (tile_type & earthgen::Terrain_tile::type_land) {
      center = vec2s{ 0.25f, 0.25f };
    } else if (tile_type & earthgen::Terrain_tile::type_water) {
      center = vec2s{ 0.75f, 0.25f };
    } else {
      center = vec2s{ 0.5f, 0.5f };
    }

    vec2s ret{ 0.25f * cos((corner_index / 6.f) * 2.f * 3.141593654f), 0.25f * sin((corner_index / 6.f) * 2.f * 3.141593654f) };
    return glms_vec2_add(ret, center);
  };

  const std::vector<std::array<vec2s, 5>>& pentagon_tex_coords() {
    static std::vector<std::array<vec2s, 5>> tex_coords{};
    static bool init = [&]() {
      for (int i = 0; i < 7; ++i) {
        tex_coords.push_back({});
        for (int j = 0; j < 5; ++j) {
          tex_coords[i][j] = pentagon_uv(i, j);
        }
      }
      return true;
    } ();

    return tex_coords;
  }

  const std::vector<std::array<vec2s, 6>>& hexagon_tex_coords() {
    static std::vector<std::array<vec2s, 6>> tex_coords{};
    static bool init = [&]() {
      for (int i = 0; i < 7; ++i) {
        tex_coords.push_back({});
        for (int j = 0; j < 6; ++j) {
          tex_coords[i][j] = hexagon_uv(i, j);
        }
      }
      return true;
    } ();

    return tex_coords;
  }

  earthgen::Planet* planet_;  
};

enum ResourceType {
  CITIZEN,
  HAPPINESS,
  MONEY,
  GOODS,
  SICKNESS,
  TAXES,
  WATER,
  POWER,
  LAND,

  IRON,
  CRYSTAL,
  COAL,

  COUNT_
};

struct Resource {
  ResourceType resource;
  union {
    int tile_id;
  } payload;
};

namespace std
{
  template<> struct hash<Resource> {
    std::size_t operator()(Resource const& resource) const noexcept {
      return std::hash<int>{}(resource.resource);
    }
  };
}

struct ResourceBin {
  Resource resource;
  uint32_t amount;
  uint32_t capacity;
};

const int MAX_UNIT_BINS = 8;
struct Unit {
  ResourceBin bins[MAX_UNIT_BINS];
};

class LogisticNode {
public:
  std::unordered_map<Resource, int> available_resources;
  std::queue<Resource> incoming_resources;
};

class LogisticNetwork {
public:
  LogisticNode root;
};

class PowerNetwork {};
class WasteNetwork {};
class WaterNetwork {};

struct PowerPlant {
  int tile;
  int radius;
};

struct ResidentialDistrict {
  float population;
  float max_population;
  bool is_powered;
  bool is_watered;
};

struct IndustrialDistrict {
  bool is_powered;
  bool is_watered;
};

struct CommercialDistrict {
  bool is_powered;
  bool is_watered;
};

struct FarmingDistrict {
  bool is_powered;
  bool is_watered;
};

struct UnitRule {
  uint16_t rate;

  ResourceBin* inputs;
  uint8_t input_count;

  ResourceBin* outputs;
  uint8_t output_count;
};

class OrbitCameraController {
public:
  OrbitCameraController(qbCamera camera, vec3s origin, float radius, float dir, float zdir) :
    camera_(camera),
    up_({ 0.f, 0.f, 1.f }),
    forward_({ 1.f, 0.f, 0.f }),
    origin_(origin),
    radius_(radius),
    dir_(dir),
    zdir_(zdir),
    rotation_(GLMS_MAT4_IDENTITY_INIT) {
    eye_ = {
      radius_ * cos(dir) * cos(zdir),
      radius_ * sin(dir) * cos(zdir),
      radius_ * sin(zdir)
    };
  }

  void move(vec3s delta) {
    origin_ = glms_vec3_add(origin_, delta);
    eye_ = glms_vec3_add(eye_, delta);

    camera_->view_mat = glms_mat4_mul(rotation_, glms_lookat(eye_, origin_, up_));
  }

  void move_to(vec3s pos) {
    vec3s delta = glms_vec3_sub(pos, origin_);
    eye_ = glms_vec3_add(eye_, delta);
    origin_ = pos;

    camera_->view_mat = glms_mat4_mul(rotation_, glms_lookat(eye_, origin_, up_));
  }

  void rotate(float angle, float x, float y, float z) {

  }

  void rotate_to(float angle, float x, float y, float z) {

  }

private:
  qbCamera camera_;
  vec3s up_;
  vec3s forward_;
  vec3s eye_;
  vec3s origin_;
  float radius_;
  float dir_;
  float zdir_;
  mat4s rotation_;
};

class Planet {
public:
  struct Tile {
    int id = -1;

    // Simulation Properties.
    int building_id = -1;
    LogisticNetwork* logistic_network = nullptr;
    PowerNetwork* power_network = nullptr;
    WasteNetwork* waste_network = nullptr;
    WaterNetwork* water_network = nullptr;

    // Render Properties.
    qbMesh highlight = nullptr;
    vec3s pos = {};
    vec3s norm = {};
  };

  struct SimulationState {
    float population;
    uint8_t pop_increase_timer;
    std::vector<Tile>* tiles;
  };

  Planet(earthgen::Planet* planet, vec3s pos, float radius):
    planet_(planet),
    position_(pos),
    orientation_(glms_mat4_identity()),
    radius_(radius),
    state_{} {

    tiles_.resize(earthgen::tiles(*planet_).size());

    PlanetMeshBuilder planet_builder(planet);

    for (auto& tile : earthgen::tiles(*planet_)) {
      Tile t{};
      t.id = earthgen::id(tile);
      t.highlight = planet_builder.tile_mesh(tile);
      t.pos = vec3s{ tile.v.x, tile.v.y, tile.v.z };

      auto corners = earthgen::corners(tile);
      vec3s v0 = { corners[0]->v.x, corners[0]->v.y, corners[0]->v.z };
      vec3s v1 = { corners[1]->v.x, corners[1]->v.y, corners[1]->v.z };
      vec3s v2 = { corners[2]->v.x, corners[2]->v.y, corners[2]->v.z };
      t.norm = glms_vec3_normalize(glms_vec3_cross(glms_vec3_sub(v0, v1), glms_vec3_sub(v2, v1)));

      tiles_[earthgen::id(tile)] = std::move(t);
    }
  }

  vec3s& position() { return position_; }
  mat4s& orientation() { return orientation_; }
  float radius() { return radius_; }

  mat4s transform() {
    return glms_translate(orientation_, position_);
  }

  Tile* select(int screen_x, int screen_y) {
    mat4s transform = GLMS_MAT4_IDENTITY_INIT;
    transform = glms_translate(transform, position_);
    transform = glms_mat4_mul(transform, orientation_);
    transform = glms_scale_uni(transform, radius_);

    Tile* ret = nullptr;

    vec3s n = qb_camera_screentoworld(active_camera, { (float)screen_x, (float)screen_y });
    qbRay_ cam_ray = { .orig = active_camera->eye, .dir = n };

    for (auto& tile : earthgen::tiles(*selected_planet)) {
      auto& corners = earthgen::corners(tile);

      vec3s tile_normal = glms_mat4_mulv3(orientation_, tiles_[tile.id].norm, 0.f);

      if (glms_vec3_dot(n, tile_normal) <= 0.f) {
        continue;
      }

      if (tile.corners.size() == 5) {
        for (auto& face : PlanetMeshBuilder::pentagon_faces()) {
          vec3s v0 = glms_mat4_mulv3(transform, { corners[face[0]]->v.x, corners[face[0]]->v.y, corners[face[0]]->v.z }, 1.f);
          vec3s v1 = glms_mat4_mulv3(transform, { corners[face[1]]->v.x, corners[face[1]]->v.y, corners[face[1]]->v.z }, 1.f);
          vec3s v2 = glms_mat4_mulv3(transform, { corners[face[2]]->v.x, corners[face[2]]->v.y, corners[face[2]]->v.z }, 1.f);

          vec3s ix_point;
          float t;
          if (qb_ray_checktri(&cam_ray, &v0, &v1, &v2, &ix_point, &t)) {
            ret = &tiles_[earthgen::id(tile)];
            std::cout << tile.id << std::endl;
            break;
          }
        }
      } else if (tile.corners.size() == 6) {
        for (auto& face : PlanetMeshBuilder::hexagon_faces()) {
          vec3s v0 = glms_mat4_mulv3(transform, { corners[face[0]]->v.x, corners[face[0]]->v.y, corners[face[0]]->v.z }, 1.f);
          vec3s v1 = glms_mat4_mulv3(transform, { corners[face[1]]->v.x, corners[face[1]]->v.y, corners[face[1]]->v.z }, 1.f);
          vec3s v2 = glms_mat4_mulv3(transform, { corners[face[2]]->v.x, corners[face[2]]->v.y, corners[face[2]]->v.z }, 1.f);

          vec3s ix_point;
          float t;
          if (qb_ray_checktri(&cam_ray, &v0, &v1, &v2, &ix_point, &t)) {
            ret = &tiles_[earthgen::id(tile)];
            std::cout << tile.id << std::endl;
            break;
          }
        }
      }

      if (ret) {
        break;
      }
    }

    return ret;
  }

  std::unordered_set<Tile*> select_around(Tile* center, int radius) {
    std::unordered_set<Tile*> ret;
    select_around_recur(center, radius, &ret);
    return ret;
  }

  static void update(SimulationState& state) {
    if (state.pop_increase_timer == 0) {
      state.population += state.population * 0.1f;
      state.population = std::roundf(state.population);
      state.pop_increase_timer = 0xFF;
    }

    --state.pop_increase_timer;

    for (auto& tile : *state.tiles) {
      std::cout << "updating " << tile.id << std::endl;
    }
  }

  const SimulationState& state() {
    return state_;
  }

  Tile& tile(int id) {
    return tiles_[id];
  }

  const std::vector<Tile>& tiles() const {
    return tiles_;
  }

private:
  void select_around_recur(Tile* center, int radius, std::unordered_set<Tile*>* selected) {
    selected->insert(center);

    if (radius < 0) {
      return;
    }    

    const earthgen::Tile& tile = earthgen::tiles(*planet_)[center->id];
    for (auto t : tile.tiles) {
      Tile* new_center = &tiles_[earthgen::id(t)];
      select_around_recur(new_center, radius - 1, selected);
    }
  }

  earthgen::Planet* planet_;
  vec3s position_;
  mat4s orientation_;
  float radius_;

  SimulationState state_;
  std::vector<Tile> tiles_;
  LogisticNetwork logistic_network_;
};

class BuildingInputController {
public:
  void on_update() {

  }
};

class PlayerController {
public:

  void on_update() {

  }
};

Planet* planet_transform;

std::unordered_set<int> buildings;
std::unordered_set<PowerPlant*> power_plants;

Planet::Tile* hover_tile = nullptr;
Planet::Tile* capital_tile = nullptr;

const char* building_type = nullptr;
const char* RESIDENTIAL_ZONE = "residential_zone";
const char* INDUSTRIAL_ZONE = "industrial_zone";
const char* COMMERCIAL_ZONE = "commercial_zone";
const char* POWER_PLANT = "power_plant";

void on_update(uint64_t frame, qbVar) {
  if (qb_key_ispressed(qbKey::QB_KEY_W)) {
    paddle_left = glms_vec2_add(paddle_left, { 0.f, -10.f });
  }
  else if (qb_key_ispressed(qbKey::QB_KEY_S)) {
    paddle_left = glms_vec2_add(paddle_left, { 0.f, 10.f });
  }

  if (qb_key_ispressed(qbKey::QB_KEY_UP)) {
    paddle_right = glms_vec2_add(paddle_right, { 0.f, -10.f });
  }
  else if (qb_key_ispressed(qbKey::QB_KEY_DOWN)) {
    paddle_right = glms_vec2_add(paddle_right, { 0.f, 10.f });
  }

  if (qb_key_ispressed(qbKey::QB_KEY_SPACE)) {
    float constant = (float)(rand() % 1000) / 100.f;
    float linear = (float)(rand() % 1000) / 10000.f;
    float quadratic = (float)(rand() % 1000) / 1000000.f;
    float lightMax = 1.f;
    float radius =
      (-linear + std::sqrtf(linear * linear - 4 * quadratic * (constant - (256.0 / 1.0) * lightMax)))
      / (2 * quadratic);
    for (int i = 0; i < 32; ++i) {

      float x = (float)(qb_rand() % qb_window_width());
      float y = (float)(qb_rand() % qb_window_height());
      float z = (float)(qb_rand() % 2000) - 1000.f;

      float r = (float)(qb_rand() % 10000) / 10000.f;
      float g = (float)(qb_rand() % 10000) / 10000.f;
      float b = (float)(qb_rand() % 10000) / 10000.f;

      qb_light_point(i, { r, g, b }, { x, y, z }, linear, quadratic, radius);
    }
  }

  if (qb_key_ispressed(qbKey::QB_KEY_1)) {
    building_type = RESIDENTIAL_ZONE;
  }

  if (qb_key_ispressed(qbKey::QB_KEY_2)) {
    building_type = INDUSTRIAL_ZONE;
  }

  if (qb_key_ispressed(qbKey::QB_KEY_3)) {
    building_type = COMMERCIAL_ZONE;
  }

  if (qb_key_ispressed(qbKey::QB_KEY_4)) {
    building_type = POWER_PLANT;
  }

  int mouse_x, mouse_y;
  qb_mouse_getposition(&mouse_x, &mouse_y);  

  if (qb_mouse_ispressed(QB_BUTTON_RIGHT)) {
    int relx, rely;
    qb_mouse_getrelposition(&relx, &rely);
    rot_speed += (float)relx * 0.005f;
    zrot_speed += (float)rely * 0.005f;
  }

  {
    mat4s& planet_orientation = planet_transform->orientation();
    planet_orientation = glms_mat4_identity();
    planet_orientation = glms_rotate(planet_orientation, zrot, { 1.f, 0.f, 0.f });
    planet_orientation = glms_rotate(planet_orientation, rot, { 0.f, 1.f, 0.f });
  }

  mat4s transform = GLMS_MAT4_IDENTITY_INIT;
  transform = glms_translate(transform, planet_transform->position());
  transform = glms_mat4_mul(transform, planet_transform->orientation());
  transform = glms_scale_uni(transform, 400.f);

  static int hover_frame_timer = 0;
  ++hover_frame_timer;
  if (hover_frame_timer == 2) {
    hover_frame_timer = 0;
    hover_tile = planet_transform->select(mouse_x, mouse_y);
  }

  if (!capital_tile && qb_mouse_ispressed(QB_BUTTON_LEFT)) {
    capital_tile = hover_tile;
  }

  if (qb_mouse_ispressed(QB_BUTTON_LEFT)) {
    auto* tile = planet_transform->select(mouse_x, mouse_y);
    if (tile) {
      if (building_type == POWER_PLANT) {
        PowerPlant* power_plant = new PowerPlant;
        power_plant->radius = 2.f;
        power_plant->tile = tile->id;
        power_plants.insert(power_plant);
      }

      if (building_type == RESIDENTIAL_ZONE) {
        PowerPlant* power_plant = new PowerPlant;
        power_plant->radius = 2.f;
        power_plant->tile = tile->id;
        power_plants.insert(power_plant);
      }

      selected_tile = &earthgen::tiles(*selected_planet)[tile->id];
      buildings.insert(tile->id);
    }
  }

  zrot += zrot_speed;
  rot -= rot_speed;
  rot_speed *= 0.9f;
  zrot_speed *= 0.9f;
}

void on_fixedupdate(uint64_t frame, qbVar) {
  static struct nk_colorf bg {
    .r = 0.10f, .g = 0.18f, .b = 0.24f, .a = 1.0f
  };

  auto ctx = nk_ctx();
  uint32_t win_width = qb_window_width();
  uint32_t win_height = qb_window_height();
  uint32_t height = 128;

  struct nk_style* s = &ctx->style;
  nk_style_push_color(ctx, &s->window.background,
    {
      (uint8_t)(bg.r * 256),
      (uint8_t)(bg.g * 256),
      (uint8_t)(bg.b * 256),
      (uint8_t)(bg.a * 256)
    });
  nk_style_push_style_item(ctx, &s->window.fixed_background, nk_style_item_color({
      (uint8_t)(bg.r * 256),
      (uint8_t)(bg.g * 256),
      (uint8_t)(bg.b * 256),
      (uint8_t)(bg.a * 256)
    }));


  if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
               NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
               NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
    enum { EASY, HARD };
    static int op = EASY;
    static int property = 20;
    nk_layout_row_static(ctx, 30, 80, 1);
    if (nk_button_label(ctx, "button"))
      printf("button pressed!\n");
    nk_layout_row_dynamic(ctx, 30, 2);
    if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
    if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;
    nk_layout_row_dynamic(ctx, 22, 1);
    nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, "background:", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 25, 1);
    if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx), 400))) {
      nk_layout_row_dynamic(ctx, 120, 1);
      bg = nk_color_picker(ctx, bg, NK_RGBA);
      nk_layout_row_dynamic(ctx, 25, 1);
      bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f, 0.005f);
      bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f, 0.005f);
      bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f, 0.005f);
      bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f, 0.005f);
      nk_combo_end(ctx);
    }
  }
  nk_end(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_style_item(ctx);
  

  if (nk_begin(ctx, "da", nk_rect(0, win_height - height, win_width, height), 0)) {
  }
  nk_end(ctx);
}

mat4s rotate_vector_to_vector(vec3s from, vec3s to) {
  vec3s axis = glms_vec3_cross(from, to);
  float angle = acos(glms_vec3_dot(from, to));
  return glms_rotate(glms_mat4_identity(), angle, axis);

  versors q{};
  vec3s a = glms_vec3_cross(from, to);
  q.x = a.x;
  q.y = a.y;
  q.z = a.z;
  q.w = sqrt((glms_vec3_norm2(from) * glms_vec3_norm2(to))) + glms_vec3_dot(from, to);
  glm_vec4_normalize(q.raw);
  return glms_quat_mat4(q);
}

void make_nodes_recur(const earthgen::Planet& planet, int max_recursion, const earthgen::Tile& seed, std::unordered_set<int>* nodes) {
  if (nodes->contains(seed.id)) {
    return;
  }

  if (planet.terrain->tiles[seed.id].type == earthgen::Terrain_tile::type_land) {
    if (qb_rand() % 100 < 70) {
      nodes->insert(seed.id);
    }
  } else {
    return;
  }

  if (max_recursion == 0) {
    return;
  }

  for (auto* tile : seed.tiles) {
    make_nodes_recur(planet, max_recursion - 1, *tile, nodes);
  }
};

void make_nodes(const earthgen::Planet& planet, int radius, const earthgen::Tile& seed, std::unordered_set<int>* nodes) {
  make_nodes_recur(planet, radius, seed, nodes);
};

int main(int, char* []) {
  qbUniverse uni = {};

  uint64_t seed = 1234;
  srand(seed);
  qb_seed(seed);

  initialize_universe(&uni);
  qb_start();

  //qb_mouse_setshow(QB_FALSE);

  float aspect = (float)qb_window_width() / (float)qb_window_height();
  qbCamera ortho = qb_camera_ortho(0.f, (float)qb_window_width(), (float)qb_window_height(), 0.f, { 0.f, 0.f });
  qbCamera perspective = qb_camera_perspective(
    45.f, aspect, 10.f, 20000.f,
    { qb_window_width() / 2.f, qb_window_height() / 2.f, -900.f },
    { qb_window_width() / 2.f, qb_window_height() / 2.f, 0.f },
    { 0.f, -1.f, 0.f });

  vec3s ball{ (float)(qb_window_width() / 2), (float)(qb_window_height() / 2), 0.f };
  
  paddle_center = (float)(qb_window_height() / 2) - PADDLE_HEIGHT / 2;
  paddle_left = { PADDLE_WIDTH, paddle_center };
  paddle_right = { (float)(qb_window_width() - PADDLE_WIDTH * 2), paddle_center };

  qbLoopCallbacks_ callbacks{ .on_update = on_update, .on_fixedupdate = on_fixedupdate };
  qbLoopArgs_ args{};

  int num_lights = 1;
  for (int i = 0; i < num_lights; ++i) {
    qb_light_enable(i, QB_LIGHT_TYPE_POINT);
  }

  qb_light_enable(0, QB_LIGHT_TYPE_DIRECTIONAL);

  float constant = 1.0;
  float linear = 0.0001;
  float quadratic = 0.00001;
  float lightMax = 1.f;
  float radius =
    (-linear + std::sqrtf(linear * linear - 4 * quadratic * (constant - (256.0 / 1.0) * lightMax)))
    / (2 * quadratic);

  qb_light_point(0, { 1.f, 1.f, 1.f }, { ((float)qb_window_width() - 25.f),  0.f, -1000.f }, linear, quadratic, radius);

  qb_light_directional(0, { 1.f, 1.f, 0.85f }, { 0.f, 0.f, 1.f }, 0.5f);

  int frame = 0;
  qbMaterial_ red_shiny{
    .albedo = {1.f, 0.1f, 0.1f},
    .metallic = 10.f,
  };

  qbMaterial_ green_shiny{
    .albedo = {0.1f, 1.f, 0.1f},
    .metallic = 10.f,
  };

  qbMaterial_ blue_shiny{
    .albedo = {0.1f, 0.1f, 1.f},
    .metallic = 10.f,
  };

  qbMaterial_ magenta_shiny{
    .albedo = {1.f, 0.1f, 1.f},
    .metallic = 10.f,
  };

  qbMaterial_ white_shiny{
    .albedo = {1.f, 1.f, 1.f},
    .metallic = 10.f,
  };
  
  qbMesh ship_mesh = qb_mesh_load("ship", "arrowhead.obj");
  qbImage planet_texture_atlas;
  {
    qbImageAttr_ attr{};
    attr.type = QB_IMAGE_TYPE_2D;
    attr.generate_mipmaps = QB_TRUE;
    qb_image_load(&planet_texture_atlas, &attr, "resources/planet_texture_atlas.png");
  }
  
  qbMesh city_block_mesh = qb_mesh_load("city_block", "city_block.obj");
  qbMesh iron_ore = qb_mesh_load("iron_ore", "iron_ore.obj");
  qbMesh crystal_ore = qb_mesh_load("crystal_ore", "crystal_ore.obj");
  qbMesh rock_ore = qb_mesh_load("rock_ore", "rock_ore.obj");
  qbMesh hex_tile = qb_mesh_load("hex_tile", "hex_tile.obj");
  qbMesh power_plant_mesh = qb_mesh_load("power_plant", "power_plant.obj");
  qbMesh capital_building_mesh = qb_mesh_load("capital_building", "capital_building.obj");

  qbMaterial_ default_material{};
  qbCamera camera = perspective;
  active_camera = camera;

  qbMaterial_ planet_material{
    .albedo = {1.f, 1.f, 1.f},
    .metallic = 1.f,

    .albedo_map = planet_texture_atlas
  };
  

  earthgen::Planet planet;
  selected_planet = &planet;
  earthgen::Terrain_parameters params{};
  params.grid_size = 5;
  params.iterations = 1000;
  earthgen::generate_terrain(planet, params);
  earthgen::Planet_colours colors;
  earthgen::init_colours(colors, planet);
  earthgen::set_colours(colors, planet, 0);

  planet_transform = new Planet(&planet, { 600.f, 400.f, 0.f }, 400.f);

  qbMesh planet_mesh;
  {
    PlanetMeshBuilder mesh_builder(&planet);
    planet_mesh = mesh_builder.planet_mesh();
  }

  int num_iron_seeds = 5;
  int num_crystal_seeds = 2;
  int num_coal_seeds = 2000;

  std::unordered_set<int> iron_ore_seeds;
  std::unordered_set<int> crystal_ore_seeds;
  std::unordered_set<int> coal_seeds;

  while (iron_ore_seeds.size() < num_iron_seeds) {
    auto& tiles = planet.grid->tiles;    
    earthgen::Tile* t = &tiles[qb_rand() % tiles.size()];
    auto id = earthgen::id(t);
    auto* terrain_tile = &earthgen::terrain(planet).tiles[id];

    while (terrain_tile->type != earthgen::Terrain_tile::type_land &&
           !crystal_ore_seeds.contains(id) && !coal_seeds.contains(id)) {
      t = &tiles[qb_rand() % tiles.size()];
      id = earthgen::id(t);
      terrain_tile = &earthgen::terrain(planet).tiles[id];
    }

    iron_ore_seeds.insert(id);
  }

  while (crystal_ore_seeds.size() < num_crystal_seeds) {
    auto& tiles = planet.grid->tiles;
    earthgen::Tile* t = &tiles[qb_rand() % tiles.size()];
    auto id = earthgen::id(t);
    auto* terrain_tile = &earthgen::terrain(planet).tiles[id];

    while (terrain_tile->type != earthgen::Terrain_tile::type_land &&
           !iron_ore_seeds.contains(id) && !coal_seeds.contains(id)) {
      t = &tiles[qb_rand() % tiles.size()];
      id = earthgen::id(t);
      terrain_tile = &earthgen::terrain(planet).tiles[id];
    }

    crystal_ore_seeds.insert(id);
  }

  while (coal_seeds.size() < num_coal_seeds) {
    auto& tiles = planet.grid->tiles;
    earthgen::Tile* t = &tiles[qb_rand() % tiles.size()];
    auto id = earthgen::id(t);
    auto* terrain_tile = &earthgen::terrain(planet).tiles[id];

    //while (//terrain_tile->type != earthgen::Terrain_tile::type_land &&
           //!crystal_ore_seeds.contains(id) && !iron_ore_seeds.contains(id)) {
      t = &tiles[qb_rand() % tiles.size()];
      id = earthgen::id(t);
      terrain_tile = &earthgen::terrain(planet).tiles[id];
    //}

    coal_seeds.insert(id);
  }

  std::unordered_set<int> iron_ore_nodes;
  std::unordered_set<int> crystal_ore_nodes;
  std::unordered_set<int> coal_ore_nodes;
  for (auto seed : iron_ore_seeds) {
    make_nodes(planet, qb_rand() % 2, earthgen::tiles(planet)[seed], &iron_ore_nodes);
  }

  for (auto seed : crystal_ore_seeds) {
    make_nodes(planet, qb_rand() % 2, earthgen::tiles(planet)[seed], &crystal_ore_nodes);
  }

  for (auto seed : coal_seeds) {
    make_nodes(planet, qb_rand() % 2, earthgen::tiles(planet)[seed], &coal_ore_nodes);
  }  

  qbMaterial_ iron_ore_material{};
  iron_ore_material.albedo = { .5f, 0.5f, 0.5f };
  iron_ore_material.metallic = 10.f;

  qbMaterial_ crystal_ore_material{};
  crystal_ore_material.albedo = { 1.f, 0.4f, 1.f };
  crystal_ore_material.metallic = 10.f;

  qbMaterial_ coal_material{};
  coal_material.albedo = { .2f, 0.2f, 0.2f };
  coal_material.metallic = 2.f;

  /*
  
  qbSprite sprite = qb_sprite_load();
  qbCamera player_cam;

  qbDrawCommands cmds = qb_draw_begin();

  for (int i = 0; i < num_asteroids; ++i) {
    qb_draw_identity(cmds);
    qb_draw_translatev(asteroids[i].pos);
    qb_draw_sprite(asterpods[i].sprite);
  }
  
  qb_draw_end(cmds);

  */

  qbCommandBatch batch;
  {
    qbDrawCommands d = qb_draw_begin();

    qb_draw_colorf(d, 1.f, 1.f, 1.f);

    qb_draw_material(d, &white_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, 0.f, 0.f, 0.f);
    qb_draw_rotatef(d, 0.f, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);

    qb_draw_material(d, &coal_material);
    for (auto& seed : coal_ore_nodes) {
      const earthgen::Tile& tile = earthgen::tiles(planet)[seed];

      qb_draw_identity(d);

      vec3s tile_pos{ tile.v.x, tile.v.y, tile.v.z };
      vec3s scaled_tile_pos = glms_vec3_scale(tile_pos, planet_transform->radius());
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 5.f);
      qb_draw_mesh(d, rock_ore);
    }

    qbDrawCompileAttr_ attr{
      .is_dynamic = true,
    };
    batch = qb_draw_compile(&d, &attr);
  }
  
  const size_t instance_count = 512;
  mat4s transforms[instance_count] = {};
  qbDrawBatch_ draw_batch {
      .mesh = rock_ore,
      .count = instance_count,
      .transforms = transforms,
  };

  while (qb_loop(&callbacks, &args) != QB_DONE) {
    qbClearValue_ clear{};
    clear.color = { 1.f, 0.f, 0.f, 1.f };

    qb_draw_beginframe(camera, &clear);
    
    qbDrawCommands d = qb_draw_begin();    

    qb_draw_colorf(d, 1.f, 1.f, 1.f);

    qb_draw_material(d, &white_shiny);
    /*qb_draw_identity(d);
    qb_draw_translatef(d, 0.f, 0.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);*/

    //qb_draw_material(d, &green_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, (float)qb_window_width() - 50.f, 0.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_sphere(d, 250.f);

    //qb_draw_material(d, &blue_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, (float)qb_window_width() - 50.f, (float)qb_window_height() - 50.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);

    //qb_draw_material(d, &magenta_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, 0.f, (float)qb_window_height() - 50.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_rotatef(d, M_PI_4, 1.f, 0.f, 0.f);
    qb_draw_rotatef(d, M_PI_4, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);
      
    qb_draw_identity(d);
    qb_draw_translatef(d, paddle_left.x, paddle_left.y, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_circle(d, 100.f);

    qb_draw_identity(d);
    qb_draw_translatef(d, paddle_right.x, paddle_right.y, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_quad(d, PADDLE_WIDTH, PADDLE_HEIGHT);
    
    /*qb_draw_identity(d);
    qb_draw_translatef(d, -(float)qb_window_width(), -(float)qb_window_height(), 100.f);
    qb_draw_colorf(d, 1.f, 1.f, 1.f);
    qb_draw_quad(d, (float)qb_window_width() * 3.f, (float)qb_window_height() * 3.f);*/

    int mouse_x, mouse_y;
    qb_mouse_getposition(&mouse_x, &mouse_y);
    vec3s n = qb_camera_screentoworld(camera, { (float)mouse_x, (float)mouse_y });

    qbRay_ cam_ray = { .orig = camera->eye, .dir = n};
    qbRay_ plane = { .orig = vec3s{}, .dir = {0.f, 0.f, 1.f} };

    float t = 0.f;
    qb_ray_checkplane(&cam_ray, &plane, &t);
    ball = glms_vec3_add(camera->eye, glms_vec3_scale(n, t));
    
    qb_draw_material(d, &planet_material);
    qb_draw_identity(d);
    qb_draw_translatev(d, planet_transform->position());
    qb_draw_multransform(d, &planet_transform->orientation());
    qb_draw_batch(d, batch);

    qb_draw_scalef(d, planet_transform->radius());
    qb_draw_mesh(d, planet_mesh);    

    qb_draw_material(d, &iron_ore_material);
    for (auto& seed : iron_ore_nodes) {
      const earthgen::Tile& tile = earthgen::tiles(planet)[seed];
      
      qb_draw_identity(d);

      vec3s tile_pos{ tile.v.x, tile.v.y, tile.v.z };
      vec3s scaled_tile_pos = glms_vec3_scale(tile_pos, planet_transform->radius());      
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 5.f);
      qb_draw_mesh(d, iron_ore);
    }

    qb_draw_material(d, &crystal_ore_material);
    for (auto& seed : crystal_ore_nodes) {
      const earthgen::Tile& tile = earthgen::tiles(planet)[seed];

      qb_draw_identity(d);

      vec3s tile_pos{ tile.v.x, tile.v.y, tile.v.z };
      vec3s scaled_tile_pos = glms_vec3_scale(tile_pos, planet_transform->radius());
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 2.5f);
      qb_draw_mesh(d, crystal_ore);
    }
    /*
    qb_draw_material(d, &coal_material);
    for (auto& seed : coal_ore_nodes) {
      const earthgen::Tile& tile = earthgen::tiles(planet)[seed];

      qb_draw_identity(d);

      vec3s tile_pos{ tile.v.x, tile.v.y, tile.v.z };
      vec3s scaled_tile_pos = glms_vec3_scale(tile_pos, planet_transform->radius());
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 5.f);
      qb_draw_mesh(d, rock_ore);
    }*/

    qb_draw_material(d, &coal_material);
    /*for (auto& tile_id : buildings) {
      const Planet::Tile& tile = planet_transform->tiles()[tile_id];
      qb_draw_material(d, &coal_material);
      qb_draw_identity(d);

      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, glms_vec3_scale(tile.norm, -0.1f));
      qb_draw_scalef(d, 400.f);
      qb_draw_mesh(d, tile.highlight);
    }*/

    qb_draw_material(d, &white_shiny);
    for (auto building : power_plants) {
      const Planet::Tile& tile = planet_transform->tiles()[building->tile];
      qb_draw_identity(d);

      vec3s scaled_tile_pos = glms_vec3_scale(tile.pos, planet_transform->radius());
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 10.f);
      qb_draw_mesh(d, power_plant_mesh);
    }

    if (!capital_tile && hover_tile) {
      qb_draw_material(d, &red_shiny);
      qb_draw_identity(d);

      vec3s scaled_tile_pos = glms_vec3_scale(hover_tile->pos, planet_transform->radius());
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 10.f);
      qb_draw_mesh(d, capital_building_mesh);
    } else if (capital_tile) {
      qb_draw_material(d, &red_shiny);
      qb_draw_identity(d);

      vec3s scaled_tile_pos = glms_vec3_scale(capital_tile->pos, planet_transform->radius());
      vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

      mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
      vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
      vec3s up = { 0.f, 1.f, 0.f };
      look_at = rotate_vector_to_vector(up, n);
      qb_draw_translatev(d, planet_transform->position());
      qb_draw_multransform(d, &planet_transform->orientation());
      qb_draw_translatev(d, scaled_tile_pos);
      qb_draw_multransform(d, &look_at);

      qb_draw_scalef(d, 10.f);
      qb_draw_mesh(d, capital_building_mesh);
    }

    if (hover_tile && false) {
      qb_draw_material(d, &red_shiny);

      for (auto tile : planet_transform->select_around(hover_tile, 1)) {
        qb_draw_identity(d);

        vec3s scaled_tile_pos = glms_vec3_scale(tile->pos, planet_transform->radius());
        vec3s planet_tile_pos = glms_vec3_add(scaled_tile_pos, planet_transform->position());

        mat4s look_at = GLMS_MAT4_IDENTITY_INIT;
        vec3s n = glms_normalize(glms_vec3_sub(planet_tile_pos, planet_transform->position()));
        vec3s up = { 0.f, 1.f, 0.f };
        look_at = rotate_vector_to_vector(up, n);
        qb_draw_translatev(d, planet_transform->position());
        qb_draw_multransform(d, &planet_transform->orientation());
        qb_draw_translatev(d, glms_vec3_negate(tile->norm));
        qb_draw_scalef(d, 400.f);
        qb_draw_mesh(d, tile->highlight);
      }
    }

    qb_draw_end(&d);

    if (frame++ % 1000 == 0) {
      qbTiming_ timing_info;
      qb_timing(uni, &timing_info);

      qb_log(QB_INFO, "%f: %f", (float)timing_info.udpate_fps, (float)timing_info.render_fps);
    }
  }
}