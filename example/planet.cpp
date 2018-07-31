#include "planet.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "render.h"
#include "shader.h"
#include "physics.h"
#include "player.h"
#include "gui.h"
#include "input.h"

#include <glm/glm.hpp>

#include <iostream>

namespace planet
{

template<class EnumType>
constexpr ItemType GetItemType(EnumType value) {
  return (ItemType)((int)value & 0xFF00);
}

struct Resource {
  ResourceType type;
  int amount;
};

struct Suicidal {
  int death_counter = 0;
};

struct Orbit {
  qbEntity parent = -1;
  float radius;
  float speed;
  float theta;
};

struct Colorable {
  qbMaterial material;
};

struct Factory {};

struct Blocker {};

enum class TileType {
  EMPTY,
  UNUSABLE,
  RESOURCE,
  BLOCKER,
  LENGTH
};

struct Tile {
  TileType type;
  union {
    Resource resource;
    Blocker blocker;
  };
};

struct Item {
  Item() : type(ItemType::NONE), amount(0) {}

  template<class EnumType>
  Item(EnumType type, int amount) : type((ItemType)((int)type)), amount(amount) {}

  ItemType type;
  int amount;
};

struct TradeShip {
  qbEntity destination;
  Item payload;
};

struct Blueprint {
  Blueprint(const std::string& name, int energy_cost, int tick_cost, std::vector<Item> requires,
            std::vector<Item> produces)
    : name(name), energy_cost(energy_cost), tick_cost(tick_cost), requires(requires),
      produces(produces) { }

  const std::string name;
  const int energy_cost;
  const int tick_cost;
  const std::vector<Item> requires;
  const std::vector<Item> produces;
};

struct TradeRoute {
  qbEntity source;
  qbEntity sink;
  qbMesh mesh;
  qbMaterial material;
  ItemType type;
};

struct Buffer {
  Buffer(size_t max_size)
    : size_(max_size), items_(new Item[max_size]),
      count_(0), head_(0), tail_(0) {}

  ~Buffer() {
    delete[] items_;
  }

  Item* At(size_t pos) {
    if (count_ == 0 || pos > count_) {
      return nullptr;
    }
    return items_ + ((head_ + pos) % size_);
  }

  void Push(Item item) {
    if (count_ == size_) { 
      return;
    }
    head_ = (head_ + 1) % size_;
    ++count_;
    items_[head_] = std::move(item);
  }

  void Pop() {
    if (count_ == 0) {
      return;
    }
    tail_ = (tail_ + 1) % size_;
    --count_;
  }

  Item* Peek() {
    if (count_ == 0) {
      return nullptr;
    }
    return items_ + tail_;
  }

  size_t Size() {
    return count_;
  }

  bool Empty() {
    return count_ == 0;
  }
  bool Full() {
    return count_ == size_;
  }

private:
  const size_t size_;
  Item* items_;

  size_t count_;
  size_t head_;
  size_t tail_;
};

struct Transporter {
  Item held;
  qbEntity sink;
};

enum class ProducerState {
  WAITING,
  PRODUCING,
};

static struct nk_image
LoadImage(const char *filename) {
  int x, y, n;
  GLuint tex;
  unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
  DEBUG_ASSERT(data, 0);

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  /*glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);*/
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
//  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);
  return nk_image_id((int)tex);
}

class Storage {
public:
  qbEntity parent;
  std::unordered_map<ItemType, Item*> items;

  Item* Find(ItemType type) {
    auto it = items.find(type);
    if (it == items.end()) {
      Item* ret = new Item(type, 0);
      items[type] = ret;
      return ret;
    }
    return it->second;
  }
};

class Building {
public:
  Building(const std::string& name, ItemType type, glm::ivec2 size):
    name_(name), type_(type), size_(size) {}

  const std::string& Name() {
    return name_;
  }

  ItemType Type() {
    return type_;
  }

  glm::ivec2 Size() {
    return size_;
  }

private:
  const std::string name_;
  ItemType type_;
  glm::ivec2 size_;

};

qbShader shader;
qbComponent orbit_component;
qbComponent colorable_component;
qbComponent trade_route_component;
qbComponent trade_ship_component;
qbComponent grid_component;
qbComponent suicidal_component;
qbComponent storage_component;
qbComponent producer_component;
qbComponent planetary_grid_component;
qbComponent transporter_component;
qbComponent buffer_component;

class Producer {
public:
  Producer(qbEntity input, qbEntity output, Blueprint* bp)
    : input_(input), output_(output), blueprint_(bp),
    state_(ProducerState::WAITING), produce_trigger_(0) {}

  static void Produce(qbInstance* insts, qbFrame*) {
    Producer** producer_p;
    qb_instance_getconst(insts[0], &producer_p);

    Producer* producer = *producer_p;
    Blueprint* bp = producer->blueprint_;

    // Cache the storage items that the producer needs.
    if (producer->required_.empty()) {
      Storage** storage_p;
      qb_instance_find(storage_component, producer->input_, &storage_p);

      producer->energy_ = (*storage_p)->Find((ItemType)ResourceType::ENERGY);
      for (const Item& item : bp->requires) {
        Item* found = (*storage_p)->Find(item.type);
        if (!found) {
          producer->required_.clear();
          return;
        }
        producer->required_.push_back(found);
      }
    }

    // Transition to making the item if the storage has enough.
    if (producer->state_ == ProducerState::WAITING) {
      bool has_enough_resources = producer->energy_->amount >= bp->energy_cost;
      for (size_t i = 0; i < bp->requires.size(); ++i) {
        has_enough_resources &= producer->required_[i]->amount >= bp->requires[i].amount;
      }

      if (!has_enough_resources) {
        return;
      }

      for (size_t i = 0; i < bp->requires.size(); ++i) {
        producer->required_[i]->amount -= bp->requires[i].amount;
      }

      producer->produce_trigger_ = 0;
      producer->state_ = ProducerState::PRODUCING;
    }

    // Wait until we have made the item.
    if (producer->state_ == ProducerState::PRODUCING) {
      ++producer->produce_trigger_;
      if (producer->produce_trigger_ >= bp->tick_cost) {

        Storage** storage_p;
        qb_instance_find(storage_component, producer->input_, &storage_p);

        for (const auto& product : bp->produces) {
          (*storage_p)->Find(product.type)->amount += product.amount;
        }

        producer->produce_trigger_ = 0;
        producer->state_ = ProducerState::WAITING;
      }
    }
  }

  static qbEntity Create(qbEntity input, qbEntity output, Blueprint* bp) {
    qbEntity ret;
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    Producer* producer = new Producer(input, output, bp);
    qb_entityattr_addcomponent(attr, producer_component, &producer);

    qb_entity_create(&ret, attr);

    qb_entityattr_destroy(&attr);

    return ret;
  }

private:
  qbEntity input_;
  qbEntity output_;

  Blueprint* blueprint_;
  Item* energy_;
  std::vector<Item*> required_;
  std::vector<Item*> produced_;

  ProducerState state_;

  int produce_trigger_;
};

class PlanetaryGrid {
public:
  struct Cell {
    glm::ivec2 pos = { 0, 0 };
    glm::ivec2 size = { 0, 0 };

    Tile tile;
    ItemType building = ItemType::NONE;
    uint8_t rotation = 0;
  };

  PlanetaryGrid(size_t grid_size) : grid_size_(grid_size) {
    cells_.resize(grid_size);
    for (std::vector<Cell>& cells : cells_) {
      cells.resize(grid_size);
    }
  }

  Cell* Parent(uint8_t x, uint8_t y) {
    Cell* cell = At(x, y);
    if (cell) {
      if ((ItemType)cell->building == ItemType::NONE) {
        return cell;
      }
      return &cells_[cell->pos.y][cell->pos.x];
    }
    return nullptr;
  }

  Cell* At(uint8_t x, uint8_t y) {
    if (IsOutOfBounds(x, y)) {
      return nullptr;
    }

    return &cells_[y][x];
  }

  bool IsEmpty(uint8_t x, uint8_t y) const {
    if (IsOutOfBounds(x, y)) {
      return false;
    }
    return (ItemType)cells_[y][x].building == ItemType::NONE;
  }

  bool IsOccupied(uint8_t x, uint8_t y) const {
    if (IsOutOfBounds(x, y)) {
      return true;
    }
    return !IsEmpty(x, y);
  }

  bool Build(ItemType building, uint8_t x, uint8_t y,
             uint8_t size_x, uint8_t size_y) {
    if (IsOutOfBounds(x + size_x - 1, y + size_y - 1)) {
      return false;
    }

    for (uint8_t i = x; i < x + size_x; ++i) {
      for (uint8_t j = y; j < y + size_y; ++j) {
        if (IsOccupied(i, j)) {
          return false;
        }
      }
    }

    for (uint8_t i = x; i < x + size_x; ++i) {
      for (uint8_t j = y; j < y + size_y; ++j) {
        Cell* cell = At(i, j);
        cell->building = building;
        cell->pos = { x, y };
        cell->size = { size_x, size_y };
        cell->rotation = 0;
      }
    }

    return true;
  }

  bool Destroy(uint8_t x, uint8_t y) {
    if (IsOutOfBounds(x, y)) {
      return false;
    }

    if (!IsOccupied(x, y)) {
      return true;
    }

    const Cell& parent = *Parent(x, y);
    glm::ivec2 pos = parent.pos;
    glm::ivec2 size = parent.size;
    for (uint8_t i = pos.x; i < pos.x + size.x; ++i) {
      for (uint8_t j = pos.y; j < pos.y + size.y; ++j) {
        Cell* cell = At(i, j);
        cell->building = ItemType::NONE;
        cell->pos = { 0, 0 };
        cell->size = { 0, 0 };
      }
    }
    return true;
  }

  size_t Size() const {
    return grid_size_;
  }

  class Builder {
  public:
    Builder(size_t grid_size) : grid_size_(grid_size) {
      cells_.resize(grid_size);
      for (std::vector<Cell>& cells : cells_) {
        cells.resize(grid_size);
        for (Cell& cell : cells) {
          cell.building = ItemType::NONE;
          cell.pos = { -1, -1 };
          cell.size = { -1, -1 };
          cell.tile.type = TileType::EMPTY;
        }
      }
    }

    void SetTile(Tile tile, uint8_t x, uint8_t y) {
      if (IsOutOfBounds(x, y)) {
        return;
      }
      *(Tile*)(&cells_[y][x].tile) = std::move(tile);
    }

    PlanetaryGrid Build() {
      PlanetaryGrid grid(grid_size_);
      grid.cells_ = cells_;
      return grid;
    }

  private:
    bool IsOutOfBounds(uint8_t x, uint8_t y) const {
      return x >= grid_size_ || y >= grid_size_;
    }

    const size_t grid_size_;
    std::vector<std::vector<Cell>> cells_;
  };

private:
  bool IsOutOfBounds(uint8_t x, uint8_t y) const {
    return x >= grid_size_ || y >= grid_size_;
  }

  const size_t grid_size_;
  std::vector<std::vector<Cell>> cells_;
  int population_;
  int energy_;
};

class Entity {
public:
  template<class... Components>
  static qbEntity Create(std::tuple<qbComponent, Components>&&... components) {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    qbResult results[] = { qb_entityattr_addcomponent(attr, std::get<0>(components), std::get<1>(components))... };

    qbEntity ret;
    qb_entity_create(&ret, attr);

    qb_entityattr_destroy(&attr);

    return ret;
  }
};

std::string ItemTypeToString(ItemType type) {
  if (GetItemType(type) == ItemType::RESOURCES) {
    ResourceType item = (ResourceType)type;
    switch (item) {
      case ResourceType::ORE:
        return "ORE";
      case ResourceType::MINERAL:
        return "MINERAL";
      case ResourceType::GAS:
        return "GAS";
      case ResourceType::WATER:
        return "WATER";
      case ResourceType::FOOD:
        return "FOOD";
      case ResourceType::ENERGY:
        return "ENERGY";
      case ResourceType::POPULATION:
        return "POPULATION";
      case ResourceType::HAPPINESS:
        return "HAPPINESS";
    }
  } else if (GetItemType(type) == ItemType::INTERMEDIATE) {
    IntermediateType item = (IntermediateType)type;
    switch (item) {
      case IntermediateType::METAL:
        return "METAL";
      case IntermediateType::CRYSTAL:
        return "CRYSTAL";
      case IntermediateType::POLYMER:
        return "POLYMER";
      case IntermediateType::CONSUMER_GOODS:
        return "CONSUMER_GOODS";
      case IntermediateType::SCIENCE:
        return "SCIENCE";
      case IntermediateType::ELECTRONICS:
        return "ELECTRONICS";
    }
  } else if (GetItemType(type) == ItemType::PRODUCTION) {
    ProductionType item = (ProductionType)type;
    switch (item) {
      case ProductionType::CITY:
        return "CITY";
      case ProductionType::ENTERTAINMENT:
        return "ENTERTAINMENT";
      case ProductionType::FARM:
        return "FARM";
      case ProductionType::MINE:
        return "MINE";
      case ProductionType::PROCESSOR:
        return "PROCESSOR";
      case ProductionType::FACTORY:
        return "FACTORY";
      case ProductionType::ACADEMY:
        return "ACADEMY";
    }
  } else if (GetItemType(type) == ItemType::LOGISTICS) {
    LogisticsType item = (LogisticsType)type;
    switch (item) {
      case LogisticsType::STORAGE:
        return "STORAGE";
    }
  }
  return "UNKNOWN";
}
ItemType StringToItemType(const std::string& type) {
  if (type == "ORE") {
    return (ItemType)ResourceType::ORE;
  } else if (type == "MINERAL") {
    return (ItemType)ResourceType::MINERAL;
  } else if (type == "GAS") {
    return (ItemType)ResourceType::GAS;
  } else if (type == "WATER") {
    return (ItemType)ResourceType::WATER;
  } else if (type == "FOOD") {
    return (ItemType)ResourceType::FOOD;
  } else if (type == "ENERGY") {
    return (ItemType)ResourceType::ENERGY;
  } else if (type == "POPULATION") {
    return (ItemType)ResourceType::POPULATION;
  } else if (type == "HAPPINESS") {
    return (ItemType)ResourceType::HAPPINESS;
  } else if (type == "METAL") {
    return (ItemType)IntermediateType::METAL;
  } else if (type == "CRYSTAL") {
    return (ItemType)IntermediateType::CRYSTAL;
  } else if (type == "POLYMER") {
    return (ItemType)IntermediateType::POLYMER;
  } else if (type == "CONSUMER_GOODS") {
    return (ItemType)IntermediateType::CONSUMER_GOODS;
  } else if (type == "SCIENCE") {
    return (ItemType)IntermediateType::SCIENCE;
  } else if (type == "ELECTRONICS") {
    return (ItemType)IntermediateType::ELECTRONICS;
  } else if (type == "CITY") {
    return (ItemType)ProductionType::CITY;
  } else if (type == "ENTERTAINMENT") {
    return (ItemType)ProductionType::ENTERTAINMENT;
  } else if (type == "FARM") {
    return (ItemType)ProductionType::FARM;
  } else if (type == "MINE") {
    return (ItemType)ProductionType::MINE;
  } else if (type == "PROCESSOR") {
    return (ItemType)ProductionType::PROCESSOR;
  } else if (type == "FACTORY") {
    return (ItemType)ProductionType::FACTORY;
  } else if (type == "ACADEMY") {
    return (ItemType)ProductionType::ACADEMY;
  } else if (type == "STORAGE") {
    return (ItemType)LogisticsType::STORAGE;
  }
  return ItemType::NONE;
}

qbEntity CreatePlanet(qbEntity parent, float planet_radius, float orbit_radius,
                      float orbit_speed, float theta,
                      PlanetaryGrid* grid,
                      glm::vec3 color = {}) {
  qbMaterial m;
  {
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setshader(attr, shader);
    qb_materialattr_setcolor(attr, { color, 1 });
    qb_material_create(&m, attr);
    qb_materialattr_destroy(&attr);
  }
  qbEntity ret;
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    physics::Transform t;
    t.p = { 0, 0, 0 };
    t.v = { 0, 0, 0 };
    qb_entityattr_addcomponent(attr, physics::component(), &t);

    Orbit orbit;
    orbit.parent = parent;
    orbit.radius = orbit_radius;
    orbit.speed = orbit_speed;
    orbit.theta = theta;
    qb_entityattr_addcomponent(attr, orbit_component, &orbit);

    Colorable colorable;
    colorable.material = m;
    qb_entityattr_addcomponent(attr, colorable_component, &colorable);

    auto builder = MeshBuilder::Sphere(planet_radius, 20, 20);
    qbMesh sphere = builder.BuildRenderable(qbRenderMode::QB_TRIANGLES);

    render::qbRenderable render_state = render::create(sphere, m);
    qb_entityattr_addcomponent(attr, render::component(), &render_state);

    physics::qbCollidable collidable;
    Mesh sphere_collision = builder.BuildMesh();
    collidable.collision_mesh = new Mesh(std::move(sphere_collision));
    qb_entityattr_addcomponent(attr, physics::collidable(), &collidable);

    /*Storage* storage = new Storage;
    for (auto&& item : items) {
      storage->items[item.type] = new Item(std::move(item));
    }
    qb_entityattr_addcomponent(attr, storage_component, &storage);*/

    qb_entityattr_addcomponent(attr, planetary_grid_component, &grid);

    qb_entity_create(&ret, attr);
    qb_entityattr_destroy(&attr);
  }

  return ret;
}

qbEntity CreateStation(qbEntity parent, float station_size, float orbit_radius,
                       float orbit_speed, float theta, glm::vec3 color = {}) {
  qbMaterial m;
  {
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setshader(attr, shader);
    qb_materialattr_setcolor(attr, { color, 1 });
    qb_material_create(&m, attr);
    qb_materialattr_destroy(&attr);
  }
  qbEntity ret;
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    physics::Transform t;
    t.p = { 0, 0, 0 };
    t.v = { 0, 0, 0 };
    qb_entityattr_addcomponent(attr, physics::component(), &t);

    Orbit orbit;
    orbit.parent = parent;
    orbit.radius = orbit_radius;
    orbit.speed = orbit_speed;
    orbit.theta = theta;
    qb_entityattr_addcomponent(attr, orbit_component, &orbit);

    Colorable colorable;
    colorable.material = m;
    qb_entityattr_addcomponent(attr, colorable_component, &colorable);

    auto builder = MeshBuilder::Box(station_size, station_size, station_size);
    qbMesh sphere = builder.BuildRenderable(qbRenderMode::QB_LINES);

    render::qbRenderable render_state = render::create(sphere, m);
    qb_entityattr_addcomponent(attr, render::component(), &render_state);

    physics::qbCollidable collidable;
    Mesh sphere_collision = builder.BuildMesh();
    collidable.collision_mesh = new Mesh(std::move(sphere_collision));
    qb_entityattr_addcomponent(attr, physics::collidable(), &collidable);

    Storage* new_storage = new Storage;
    qb_entityattr_addcomponent(attr, storage_component, &new_storage);
    
    PlanetaryGrid::Builder grid_builder(8);
    PlanetaryGrid* grid = new PlanetaryGrid(std::move(grid_builder.Build()));
    qb_entityattr_addcomponent(attr, planetary_grid_component, &grid);

    qb_entity_create(&ret, attr);
    qb_entityattr_destroy(&attr);
  }
  return ret;
}
qbEntity CreateTradeRoute(qbEntity source, qbEntity sink, ItemType type) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  qbMesh mesh;
  {
    MeshBuilder builder;
    builder.AddVertex({ 0, 0, 0 });
    builder.AddVertex({ 1, 0, 0 });
    mesh = builder.BuildRenderable(qbRenderMode::QB_LINES);
  }

  qbMaterial material;
  {
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setshader(attr, shader);
    qb_materialattr_setcolor(attr, { 1, 1, 1, 1 });
    qb_material_create(&material, attr);
    qb_materialattr_destroy(&attr);
  }

  //render::qbRenderable renderable;
  //renderable = render::create(mesh, material);
  //qb_entityattr_addcomponent(attr, render::component(), &renderable);

  physics::Transform t;
  qb_entityattr_addcomponent(attr, physics::component(), &t);

  TradeRoute trade_route;
  trade_route.source = source;
  trade_route.sink = sink;
  trade_route.type = type;
  trade_route.mesh = mesh;
  trade_route.material = material;
  qb_entityattr_addcomponent(attr, trade_route_component, &trade_route);

  qbEntity ret;
  qb_entity_create(&ret, attr);
  qb_entityattr_destroy(&attr);

  return ret;
}

qbMaterial dot_material;
qbMesh dot_mesh;
render::qbRenderable dot_renderable;

std::unordered_map<std::string, Blueprint*> blueprints;
std::unordered_map<ItemType, Building*> buildings;

void InitializeBlueprints(std::unordered_map<std::string, Blueprint*>* bps) {
  (*bps)["METAL"] = new Blueprint(
    "ORE PROCESSING",
    1,  // Energy cost
    1,  // Tick cost
    { { ResourceType::ORE, 1 } },  // Required resources
    { { IntermediateType::METAL, 1 } }  // Produces
  );

  (*bps)["CRYSTAL"] = new Blueprint(
    "CRYSTAL PROCESSING",
    1,  // Energy cost
    1,  // Tick cost
    { { ResourceType::MINERAL, 1 } },  // Required resources
    { { IntermediateType::CRYSTAL, 1 } }  // Produces
  );

  (*bps)["POLYMER"] = new Blueprint(
    "GAS PROCESSING",
    1,  // Energy cost
    1,  // Tick cost
    { { ResourceType::GAS, 1 } },  // Required resources
    { { IntermediateType::POLYMER, 1 } }  // Produces
  );

  (*bps)["ELECTRONICS"] = new Blueprint(
    "ELECTRONICS MANUFACTURING",
    1,  // Energy cost
    1,  // Tick cost
    { { IntermediateType::METAL, 1 },
      { IntermediateType::CRYSTAL, 1 } },  // Required products
    { { IntermediateType::ELECTRONICS, 1 } }  // Produces
  );

  (*bps)["FACTORY"] = new Blueprint(
    "FACTORY MANUFACTURING",
    10,  // Energy cost
    10,  // Tick cost
    { { IntermediateType::METAL, 10 },
      { IntermediateType::CRYSTAL, 10 } },  // Required products
    { { ProductionType::FACTORY, 1 } }  // Produces
  );

  (*bps)["SOLAR_PLANT"] = new Blueprint(
    "SOLAR GENERATOR",
    0,  // Energy cost
    100,  // Tick cost
    { {} },  // Required resources
    { { ResourceType::ENERGY, 10 } }  // Produces
  );
}

void InitializeBuildings(std::unordered_map<ItemType, Building*>* buildings) {
  (*buildings)[(ItemType)ProductionType::MINE] = new Building(
    "MINE",
    (ItemType)ProductionType::MINE,
    glm::ivec2{ 2, 2 }
  );

  (*buildings)[(ItemType)ProductionType::PROCESSOR] = new Building(
    "PROCESSING FACILITY",
    (ItemType)ProductionType::PROCESSOR,
    glm::ivec2{ 2, 2 }
  );

  (*buildings)[(ItemType)ProductionType::CITY] = new Building(
    "CITY",
    (ItemType)ProductionType::CITY,
    glm::ivec2{ 2, 2 }
  );

  (*buildings)[(ItemType)ProductionType::FARM] = new Building(
    "FARM",
    (ItemType)ProductionType::FARM,
    glm::ivec2{ 2, 2 }
  );


  (*buildings)[(ItemType)ProductionType::ACADEMY] = new Building(
    "ACADEMY",
    (ItemType)ProductionType::ACADEMY,
    glm::ivec2{ 4, 4 }
  );

  (*buildings)[(ItemType)ProductionType::FACTORY] = new Building(
    "FACTORY",
    (ItemType)ProductionType::FACTORY,
    glm::ivec2{ 3, 3 }
  );

  (*buildings)[(ItemType)ProductionType::SOLAR_PLANT] = new Building(
    "SOLAR GENERATOR",
    (ItemType)ProductionType::SOLAR_PLANT,
    glm::ivec2{ 2, 2 }
  );

  (*buildings)[(ItemType)LogisticsType::ROCKET_SILO] = new Building(
    "ROCKET SILO",
    (ItemType)LogisticsType::ROCKET_SILO,
    glm::ivec2{ 4, 4 }
  );

  (*buildings)[(ItemType)LogisticsType::TRANSPORTER] = new Building(
    "TRANSPORTER",
    (ItemType)LogisticsType::TRANSPORTER,
    glm::ivec2{ 1, 1 }
  );

  (*buildings)[(ItemType)LogisticsType::STORAGE] = new Building(
    "STORAGE",
    (ItemType)LogisticsType::STORAGE,
    glm::ivec2{ 1, 1 }
  );

  (*buildings)[(ItemType)LogisticsType::INSERTER] = new Building(
    "INSERTER",
    (ItemType)LogisticsType::INSERTER,
    glm::ivec2{ 1, 1 }
  );

  (*buildings)[(ItemType)ProductionType::ADMINISTRATION] = new Building(
    "ADMINISTRATION",
    (ItemType)ProductionType::ADMINISTRATION,
    glm::ivec2{ 5, 5 }
  );

  (*buildings)[(ItemType)LogisticsType::STATION_DOCK] = new Building(
    "DOCK",
    (ItemType)LogisticsType::STATION_DOCK,
    glm::ivec2{ 2, 2 }
  );
}

qbEntity CreateTransporter(qbEntity sink) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Transporter t;
  t.sink = sink;
  qb_entityattr_addcomponent(attr, transporter_component, &t);

  Buffer buffer(1);
  qb_entityattr_addcomponent(attr, buffer_component, &buffer);

  qbEntity ret;
  qb_entity_create(&ret, attr);

  qb_entityattr_destroy(&attr);

  return ret;
}

qbEntity CreateProducer(qbEntity input, qbEntity output, const std::string& bp) {
  Blueprint* blueprint = blueprints[bp];
  if (blueprint) {
    return Producer::Create(input, output, blueprint);
  }
  return -1;
}

struct nk_image transporter_img_u;
struct nk_image transporter_img_d;
struct nk_image transporter_img_l;
struct nk_image transporter_img_r;
struct nk_image mine_img;

void Initialize(const Settings& settings) {
  InitializeBlueprints(&blueprints);
  InitializeBuildings(&buildings);

  shader = settings.shader;
  {
    transporter_img_u = LoadImage((settings.resource_folder + "transporter_up.png").c_str());
    transporter_img_d = LoadImage((settings.resource_folder + "transporter_down.png").c_str());
    transporter_img_l = LoadImage((settings.resource_folder + "transporter_left.png").c_str());
    transporter_img_r = LoadImage((settings.resource_folder + "transporter_right.png").c_str());
    mine_img = LoadImage((settings.resource_folder + "mine.png").c_str());
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Orbit);
    qb_component_create(&orbit_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Colorable);
    qb_component_create(&colorable_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, TradeRoute);
    qb_component_create(&trade_route_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, TradeShip);
    qb_component_create(&trade_ship_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Suicidal);
    qb_component_create(&suicidal_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Storage*);
    qb_component_create(&storage_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Producer*);
    qb_component_create(&producer_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, PlanetaryGrid*);
    qb_component_create(&planetary_grid_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Buffer);
    qb_component_create(&buffer_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Transporter);
    qb_component_create(&transporter_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setshader(attr, shader);
    qb_materialattr_setcolor(attr, { 1, 1, 1, 1 });
    qb_material_create(&dot_material, attr);
    qb_materialattr_destroy(&attr);
  }
  {
    MeshBuilder builder;
    builder.AddVertex({ 0, 0, 0 });
    dot_mesh = builder.BuildRenderable(qbRenderMode::QB_POINTS);
  }
  {
    dot_renderable = render::create(dot_mesh, dot_material);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, transporter_component);
    qb_systemattr_addmutable(attr, buffer_component);
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      Transporter* transport;
      qb_instance_getconst(insts[0], &transport);

      Buffer* buffer;
      qb_instance_getmutable(insts[1], &buffer);

      if (transport->sink >= 0) {
        Buffer *sink;
        qb_instance_find(buffer_component, transport->sink, &sink);

        if (transport->held.type != ItemType::NONE && !sink->Full()) {
          sink->Push(transport->held);
          transport->held = Item();
        }
      }

      if (transport->held.type == ItemType::NONE && !buffer->Empty()) {
        Item* to_move = buffer->Peek();
        transport->held = std::move(*to_move);
        buffer->Pop();
      }
    });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, orbit_component);
    qb_systemattr_addmutable(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      Orbit* orbit;
      qb_instance_getmutable(insts[0], &orbit);

      physics::Transform* t;
      qb_instance_getmutable(insts[1], &t);

      t->p = orbit->radius * glm::vec3{ glm::cos(glm::radians(orbit->theta)),  glm::sin(glm::radians(orbit->theta)), 0 };

      if (orbit->parent >= 0) {
        physics::Transform* parent_transform;
        qb_instance_find(physics::component(), orbit->parent, &parent_transform);

        t->p += parent_transform->p;
      }
      orbit->theta += orbit->speed;
    });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    static int trigger = 0;

    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, trade_route_component);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      TradeRoute* trade_route;
      qb_instance_getmutable(insts[0], &trade_route);

      physics::Transform* source;
      physics::Transform* sink;
      qb_instance_find(physics::component(), trade_route->source, &source);
      qb_instance_find(physics::component(), trade_route->sink, &sink);

      auto v_vbo = qb_mesh_getvbuffer(trade_route->mesh);
      glm::vec3 new_verts[] = { source->p, sink->p };
      
      if (false && trigger > 2) {
        render::qb_render_makecurrent();
        glBindVertexArray(trade_route->mesh->vao);
        glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
        glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(glm::vec3),
                     new_verts, GL_DYNAMIC_DRAW);
        render::qb_render_makenull();
      }

      if (trigger % 100 == 0) {
        Storage** source_resources;
        Storage** sink_resources;
        qb_instance_find(storage_component, trade_route->source, &source_resources);
        qb_instance_find(storage_component, trade_route->sink, &sink_resources);

        Item* found = (*source_resources)->Find(trade_route->type);
        if (found->amount <= 0) {
          return;
        }

        TradeShip ship;
        ship.payload = Item(trade_route->type, 1);
        ship.destination = trade_route->sink;

        found->amount = std::max(found->amount - 1, 0);

        qbEntityAttr attr;
        qb_entityattr_create(&attr);

        physics::Transform t;
        t.is_fixed = false;
        t.p = source->p;
        t.v = glm::normalize(sink->p - source->p) * 0.05f;
        qb_entityattr_addcomponent(attr, physics::component(), &t);

        qb_entityattr_addcomponent(attr, render::component(), &dot_renderable);

        Suicidal suicidal;
        suicidal.death_counter = (int)(glm::length(sink->p - source->p) / glm::length(t.v));
        qb_entityattr_addcomponent(attr, suicidal_component, &suicidal);

        qb_entityattr_addcomponent(attr, trade_ship_component, &ship);

        qbEntity unused;
        qb_entity_create(&unused, attr);
        qb_entityattr_destroy(&attr);
      }
    });
    qb_systemattr_setcallback(attr, [](qbFrame*) {
      ++trigger;
    });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, suicidal_component);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      Suicidal* suicidal;
      qb_instance_getmutable(insts[0], &suicidal);
      --suicidal->death_counter;
      if (suicidal->death_counter < 0) {
        qb_entity_destroy(qb_instance_getentity(insts[0]));
      }
    });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qb_instance_oncreate(trade_ship_component, [](qbInstance instance) {
      TradeShip* ship;
      qb_instance_getcomponent(instance, trade_ship_component, &ship);

      physics::Transform* source;
      qb_instance_getcomponent(instance, physics::component(), &source);

      physics::Transform* destination;
      qb_instance_find(physics::component(), ship->destination, &destination);
    });

    qb_instance_ondestroy(trade_ship_component, [](qbInstance instance) {
      TradeShip* ship;
      qb_instance_getcomponent(instance, trade_ship_component, &ship);
      
      Storage** sink;
      qb_instance_find(storage_component, ship->destination, &sink);

      ((*sink)->Find(ship->payload.type))->amount += ship->payload.amount;
    });
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, producer_component);
    qb_systemattr_setfunction(attr, Producer::Produce);

    qbSystem unused;
    qb_system_create(&unused, attr);

    qb_systemattr_destroy(&attr);
  }

  qbEntity sun;
  {
    size_t planet_size = 32;
    PlanetaryGrid::Builder builder(planet_size);
    for (uint8_t x = 0; x < planet_size; ++x) {
      for (uint8_t y = 0; y < planet_size; ++y) {
        Tile tile;
        tile.type = TileType::RESOURCE;
        tile.resource.type = ResourceType::ENERGY;
        tile.resource.amount = 1000000000;
        builder.SetTile(tile, x, y);
      }
    }

    PlanetaryGrid* grid = new PlanetaryGrid(std::move(builder.Build()));
    sun = CreatePlanet(
      -1,  // parent
      20,  // planet_radius
      0,  // orbit_radius
      0.00001f,  // orbit_speed
      0,  // theta
      grid,
      { 1, 1, 0 }  // color
    );
  }

  qbEntity estation_1 = CreateStation(sun, 1, 50, 0.0005f, 90, glm::vec3{ 1, 1, 0 });
  qbEntity estation_2 = CreateStation(sun, 1, 50, 0.0001f, 270, glm::vec3{ 1, 1, 0 });

  qbEntity planet_entity;
  {
    size_t planet_size = 32;
    PlanetaryGrid::Builder builder(planet_size);
    for (uint8_t x = 0; x < planet_size; ++x) {
      for (uint8_t y = 0; y < planet_size; ++y) {
        if (rand() % 100 < 10) {
          Tile tile;
          tile.type = TileType::RESOURCE;
          if (rand() % 100 < 50) {
            tile.resource.type = ResourceType::FOOD;
            tile.resource.amount = 1000000000;
          } else {
            if (rand() % 100 < 50) {
              tile.resource.type = ResourceType::ORE;
              tile.resource.amount = 1000000000;
            } else {
              tile.resource.type = ResourceType::MINERAL;
              tile.resource.amount = 1000000000;
            }
          }
          builder.SetTile(tile, x, y);
        }
      }
    }

    PlanetaryGrid* grid = new PlanetaryGrid(std::move(builder.Build()));
    planet_entity = CreatePlanet(
      sun,  // parent
      10,  // planet_radius
      75,  // orbit_radius
      0.0005,  // orbit_speed
      90,  // theta
      grid,
      { 0, 1, 0 }  // color
    );
  }

  // Metal factory.
  //Producer::Create(planet_entity, blueprints["METAL"]);

  qbEntity moon_entity;
  {
    size_t planet_size = 8;
    PlanetaryGrid::Builder builder(planet_size);
    for (uint8_t x = 0; x < planet_size; ++x) {
      for (uint8_t y = 0; y < planet_size; ++y) {
        if (rand() % 100 < 10) {
          Tile tile;
          tile.type = TileType::RESOURCE;
          if (rand() % 100 < 50) {
            if (rand() % 100 < 50) {
              tile.resource.type = ResourceType::ORE;
              tile.resource.amount = 1000000000;
            } else {
              tile.resource.type = ResourceType::MINERAL;
              tile.resource.amount = 1000000000;
            }
          }
          builder.SetTile(tile, x, y);
        }
      }
    }

    PlanetaryGrid* grid = new PlanetaryGrid(std::move(builder.Build()));
    moon_entity = CreatePlanet(
      planet_entity,  // parent
      1,  // planet_radius
      15,  // orbit_radius
      0.002,  // orbit_speed
      180,  // theta
      grid,
      { 0.4, 0.4, 0.4 }  // color
    );
  }


  qbEntity moon_station_entity = CreateStation(planet_entity, 0.05f, 0.125f, 2, 180, { 0.2, 0.2, 0.2 });

  qbEntity other_planet_entity;
  {
    size_t planet_size = 32;
    PlanetaryGrid::Builder builder(planet_size);
    for (uint8_t x = 0; x < planet_size; ++x) {
      for (uint8_t y = 0; y < planet_size; ++y) {
        if (rand() % 100 < 10) {
          Tile tile;
          tile.type = TileType::RESOURCE;
          tile.resource.type = ResourceType::GAS;
          tile.resource.amount = 1000000000;
          builder.SetTile(tile, x, y);
        }
      }
    }

    PlanetaryGrid* grid = new PlanetaryGrid(std::move(builder.Build()));
    other_planet_entity = CreatePlanet(
      sun,  // parent
      5,  // planet_radius
      150,  // orbit_radius
      0.0001f,  // orbit_speed
      270,  // theta
      grid,
      { 0, 0, 1 }  // color
    );
  }
  //CreateTradeRoute(estation_1, planet_entity, (ItemType)ResourceType::ENERGY);
  //CreateTradeRoute(estation_2, other_planet_entity, (ItemType)ResourceType::ENERGY);
  //Producer::Create(estation_1, blueprints["SOLAR_PLANT"]);
  //Producer::Create(estation_2, blueprints["SOLAR_PLANT"]);

  //CreateTradeRoute(planet_entity, moon_entity, ResourceType::FOOD);
  //CreateTradeRoute(planet_entity, moon_entity, (ItemType)IntermediateType::METAL);
  //CreateTradeRoute(moon_entity, other_planet_entity, (ItemType)IntermediateType::METAL);
}

std::string Description(qbEntity entity) {
  if (qb_entity_hascomponent(entity, storage_component)) {
    Storage** storage;
    qb_instance_find(storage_component, entity, &storage);
    return std::to_string((*storage)->Find((ItemType)IntermediateType::METAL)->amount);
  }
  return "";
}
/*
void UICreateTradeRoute() {
  static planet::ItemType trade_route_type = planet::ItemType::NONE;
  static float payload_amount = 0.0f;
  ImGui::InputFloat("Payload Amount", &payload_amount);
  if (ImGui::BeginMenu("Create Trade Route")) {
    if (ImGui::BeginMenu("Resources")) {
      if (ImGui::MenuItem("Ore")) trade_route_type = (ItemType)ResourceType::ORE;
      if (ImGui::MenuItem("Mineral")) trade_route_type = (ItemType)ResourceType::MINERAL;
      if (ImGui::MenuItem("Gas")) trade_route_type = (ItemType)ResourceType::GAS;
      if (ImGui::MenuItem("Water")) trade_route_type = (ItemType)ResourceType::WATER;
      if (ImGui::MenuItem("Food")) trade_route_type = (ItemType)ResourceType::FOOD;
      if (ImGui::MenuItem("Energy")) trade_route_type = (ItemType)ResourceType::ENERGY;
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Itermediade Products")) {
      if (ImGui::MenuItem("Metal")) trade_route_type = (ItemType)IntermediateType::METAL;
      if (ImGui::MenuItem("Crystal")) trade_route_type = (ItemType)IntermediateType::CRYSTAL;
      if (ImGui::MenuItem("Polymer")) trade_route_type = (ItemType)IntermediateType::POLYMER;
      if (ImGui::MenuItem("Consumer Goods")) trade_route_type = (ItemType)IntermediateType::CONSUMER_GOODS;
      if (ImGui::MenuItem("Science")) trade_route_type = (ItemType)IntermediateType::SCIENCE;
      if (ImGui::MenuItem("Electronics")) trade_route_type = (ItemType)IntermediateType::ELECTRONICS;
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Production")) {
      if (ImGui::MenuItem("City")) trade_route_type = (ItemType)ProductionType::CITY;
      if (ImGui::MenuItem("Entertainment")) trade_route_type = (ItemType)ProductionType::ENTERTAINMENT;
      if (ImGui::MenuItem("Farm")) trade_route_type = (ItemType)ProductionType::FARM;
      if (ImGui::MenuItem("Mine")) trade_route_type = (ItemType)ProductionType::MINE;
      if (ImGui::MenuItem("Processing Center")) trade_route_type = (ItemType)ProductionType::PROCESSING;
      if (ImGui::MenuItem("Factory")) trade_route_type = (ItemType)ProductionType::FACTORY;
      if (ImGui::MenuItem("Academy")) trade_route_type = (ItemType)ProductionType::ACADEMY;
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Logistics")) {
      if (ImGui::MenuItem("Storage")) trade_route_type = (ItemType)LogisticsType::STORAGE;
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }

  auto selected = player::SelectedEntityMouseDrag();
  ImGui::Text("Source: %d", selected.first);
  ImGui::Text("Destination: %d", selected.second);

  if (trade_route_type != planet::ItemType::NONE) {
    planet::CreateTradeRoute(selected.first, selected.second, trade_route_type);
    trade_route_type = planet::ItemType::NONE;
  }
  ImGui::Separator();
}
*/

Building* SelectBuildingComboBoxWidget() {
  static Building* selected_building = nullptr;

  struct nk_rect space;
  enum nk_widget_layout_states state;
  state = nk_widget(&space, gui::Context());
  nk_layout_row_dynamic(gui::Context(), 30, 1);
  nk_label(gui::Context(), "Buildings:", NK_TEXT_LEFT);
  if (nk_combo_begin_label(gui::Context(), selected_building ? selected_building->Name().c_str() : "", nk_vec2(nk_widget_width(gui::Context()), 200))) {
    nk_layout_row_dynamic(gui::Context(), 25, 1);
    for (const auto& building_pair : buildings) {
      if (nk_combo_item_label(gui::Context(), building_pair.second->Name().c_str(), NK_TEXT_LEFT)) {
        selected_building = building_pair.second;
      }
    }
    nk_combo_end(gui::Context());
  }
  return selected_building;
}

glm::ivec2 PlanetaryGridWidget(qbEntity selected, Building* selected_building, int draw_size) {
  static int rotation = 0;

  if (!qb_entity_hascomponent(selected, planetary_grid_component)) {
    return{ -1, -1 };
  }

  PlanetaryGrid** pgrid;
  qb_instance_find(planetary_grid_component, selected, &pgrid);
  PlanetaryGrid* grid = *pgrid;

  nk_context* ctx = gui::Context();
  nk_command_buffer* canvas = nk_window_get_canvas(ctx);
  nk_input* input = &ctx->input;
  
  ctx->style.window.spacing = nk_vec2(1, 1);
  ctx->style.window.padding = nk_vec2(8, 8);
  int grid_size = grid->Size();
  nk_layout_row_static(ctx, draw_size, draw_size, grid_size);
  
  glm::ivec2 ret{ -1, -1 };

  if (input::is_key_pressed(qbKey::QB_KEY_R)) {
    rotation = (rotation + 1) % 4;
  }

  for (size_t i = 0; i < grid_size * grid_size; ++i) {
    struct nk_rect space;
    enum nk_widget_layout_states state;
    state = nk_widget(&space, ctx);
    if (!state) return { -1, -1 };
    if (state != NK_WIDGET_ROM) {
      //update_your_widget_by_user_input(...);
    }

    PlanetaryGrid::Cell* cell = grid->At(i % grid_size, i / grid_size);
    nk_color color = nk_rgb(0, 0, 0);
    if (cell) {
      switch (cell->tile.type) {
        case TileType::BLOCKER:
          color = nk_rgb(255, 0, 0);
          break;
        case TileType::UNUSABLE:
          color = nk_rgb(255, 0, 0);
          break;
        case TileType::RESOURCE: {
          const Resource& resource = cell->tile.resource;
          switch (resource.type) {
            case ResourceType::ORE:
              color = nk_rgb(102, 89, 65);
              break;
            case ResourceType::MINERAL:
              color = nk_rgb(89, 186, 224);
              break;
            case ResourceType::GAS:
              color = nk_rgb(188, 188, 188);
              break;
            case ResourceType::WATER:
              color = nk_rgb(0, 0, 255);
              break;
            case ResourceType::FOOD:
              color = nk_rgb(0, 255, 0);
              break;
            case ResourceType::ENERGY:
              color = nk_rgb(255, 255, 0);
              break;
          }
        } break;
      }
      if ((ItemType)cell->building == ItemType::NONE) {
        nk_fill_rect(canvas, space, 0.0f, color);
      } else {
        glm::ivec2 pos = { i % grid_size, i / grid_size };
        PlanetaryGrid::Cell* parent = grid->Parent(pos.x, pos.y);
        mine_img.w = parent->size.x;
        mine_img.h = parent->size.y;
        mine_img.region[0] = pos.x - parent->pos.x;
        mine_img.region[1] = pos.y - parent->pos.y;
        mine_img.region[2] = 1;
        mine_img.region[3] = 1;

        nk_draw_rotated_image(canvas, space, 0.0f, &mine_img, nk_rgb(255, 255, 255));
        //nk_draw_image(canvas, space, &transporter_img_u, nk_rgb(255, 255, 255));
        color = nk_rgb(255, 0, 255);
      }
    }

    if (state == NK_WIDGET_ROM) {
    } else if (state == NK_WIDGET_VALID) {
      color = nk_rgb(0, 255, 0);
      ret = { i % grid_size, i / grid_size };
      if (nk_input_mouse_clicked(&ctx->input, nk_buttons::NK_BUTTON_LEFT, space) && selected_building) {
        grid->Build(selected_building->Type(), ret.x, ret.y, selected_building->Size().x, selected_building->Size().y);
      }

      if (nk_input_mouse_clicked(&ctx->input, nk_buttons::NK_BUTTON_RIGHT, space)) {
        grid->Destroy(ret.x, ret.y);
      }
    }    


    
  }
  return ret;
}

void RenderPlanetPopup() {
  qbEntity selected = player::SelectedEntity();
  if (selected >= 0) {
    static Building* selected_building = nullptr;
    if (nk_begin(gui::Context(), "Planetary Grid", nk_rect(10, 10, 400, 600),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE )) {
      if (selected >= 0) {
        char buf[128] = { 0 };
        sprintf_s(buf, 128, "Planet: %d", selected);
        nk_layout_row_dynamic(gui::Context(), 20, 1);
        nk_label(gui::Context(), buf, NK_TEXT_LEFT);
      } else {
        nk_layout_row_dynamic(gui::Context(), 20, 1);
        nk_label(gui::Context(), "No planet selected", NK_TEXT_LEFT);
      }
      PlanetaryGridWidget(selected, selected_building, 10);
      selected_building = SelectBuildingComboBoxWidget();
    }
    nk_end(gui::Context());
  }
}

}