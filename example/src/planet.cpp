#include "planet.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "render.h"
#include "shader.h"
#include "physics.h"
#include "player.h"
#include "gui.h"
#include "input.h"
#include "items.h"

#include <glm/glm.hpp>

#include <iostream>
#include <memory>

#define InitializeComponent(component, data) \
{ \
qbComponentAttr attr; \
qb_componentattr_create(&attr); \
qb_componentattr_setdatatype(attr, data); \
qb_component_create(component, attr); \
qb_componentattr_destroy(&attr); \
}

#define InitializeComponentWithType(component, data, type) \
{ \
qbComponentAttr attr; \
qb_componentattr_create(&attr); \
qb_componentattr_setdatatype(attr, data); \
qb_componentattr_settype(attr, type); \
qb_component_create(component, attr); \
qb_componentattr_destroy(&attr); \
}

namespace planet
{

namespace
{

glm::ivec2 BuildingRotationToPoint(uint8_t rotation) {
  if (rotation % 4 == 0) {
    return{ 0, -1 };
  } else if (rotation % 4 == 1) {
    return{ 1, 0 };
  } else if (rotation % 4 == 2) {
    return{ 0, 1 };
  } else if (rotation % 4 == 3) {
    return{ -1, 0 };
  }
  return{ 0, 0 };
}

GLuint LoadImage(const std::string&) {
#if 0
  int x, y, n;
  GLuint tex;
  unsigned char *data = stbi_load(filename.c_str(), &x, &y, &n, 0);
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
  //stbi_image_free(data);
  return tex;
#endif
  return 0;
}

}

qbShader shader;
qbComponent orbit_component;
qbComponent colorable_component;
qbComponent trade_route_component;
qbComponent trade_ship_component;
qbComponent suicidal_component;
qbComponent storage_component;
qbComponent input_storage_component;
qbComponent output_storage_component;
qbComponent producer_component;
qbComponent planetary_grid_component;
qbComponent transporter_component;
qbComponent buffer_component;
qbComponent mine_component;
qbComponent item_component;
qbComponent item_list_component;
qbComponent cell_ref_component;
qbComponent administration_component;

struct Orbit {
  qbEntity parent = -1;
  float radius;
  float speed;
  float theta;
};

struct Resource {
  ResourceType type;
  int amount;
};

struct Suicidal {
  int death_counter = 0;
};

struct Colorable {
  qbMaterial material;
};

enum class TileType {
  EMPTY,
  UNUSABLE,
  RESOURCE,
  BLOCKER,
  LENGTH
};

struct Tile {
  TileType type;
  Resource resource;
};

struct Item {
  Item() : type(ItemType::NONE), amount(0) {}

  template<class EnumType>
  Item(EnumType type, int amount) : type((ItemType)((int)type)), amount(amount) {}

  bool operator<(const Item& other) const {
    return type < other.type;
  }

  ItemType type;
  size_t amount;
};

struct TradeShip {
  qbEntity destination;
  Item payload;
};

struct Blueprint {
  Blueprint(const std::string& name, int energy_cost, int tick_cost, std::set<Item> requires,
            std::set<Item> produces)
    : name(name), energy_cost(energy_cost), tick_cost(tick_cost), requires(requires),
    produces(produces) {}

  const std::string name;
  const int energy_cost;
  const int tick_cost;
  const std::set<Item> requires;
  const std::set<Item> produces;
};

struct TradeRoute {
  qbEntity source;
  qbEntity sink;
  qbMesh mesh;
  qbMaterial material;
  ItemType type;
};

struct Buffer {
  Buffer() : capacity_(0), filter_(ItemType::NONE) {}
  Buffer(Item item, size_t capacity, ItemType filter = ItemType::ANY)
    : capacity_(capacity), filter_(filter), item_(item) {}
  Buffer(size_t capacity, ItemType filter = ItemType::ANY)
    : capacity_(capacity), filter_(filter) {}

  bool operator<(const Buffer& other) const {
    return filter_ < other.filter_;
  }

  ItemType Type() const {
    return filter_;
  }

  // Returns the amount of added items. Subtracts that amount from item.
  size_t Push(Item* item) {
    // A filter of ItemType::ANY means anything can be held in the buffer.
    if (item->type != filter_ && filter_ != ItemType::ANY) {
      return 0;
    }

    // Only push if we have matching types. Allow arbitrary pushing if the buffer is empty.
    if (item_.type != item->type && !Empty()) {
      return 0;
    }

    size_t new_amount = std::min(capacity_, item->amount + item_.amount);
    size_t amount_added = new_amount - item_.amount;
    item_.amount = new_amount;
    item_.type = item->type;
    item->amount -= amount_added;

    return amount_added;
  }

  size_t Push(Buffer* buffer) {
    return Push(&buffer->item_);
  }

  // Returns the amount of removed items.
  Item Pull(size_t amount) {
    amount = std::min(amount, item_.amount);
    item_.amount -= amount;
    return Item(item_.type, amount);
  }

  Item* Peek() {
    if (Empty()) {
      return nullptr;
    }
    return &item_;
  }

  size_t Capacity() {
    return capacity_;
  }

  size_t Size() {
    return item_.amount;
  }

  bool Empty() {
    return item_.amount == 0;
  }

  bool Full() {
    return item_.amount == capacity_;
  }

private:
  size_t capacity_;
  ItemType filter_;
  Item item_;
};

struct CellRef {
  class PlanetaryGrid* grid;
  glm::ivec2 pos;
};

struct Transporter {
  glm::ivec2 source;
  glm::ivec2 sink;
  size_t push_speed = 1;
  size_t pull_speed = 1;
};

struct Storage {
private:
  std::unordered_map<ItemType, Buffer> buffers_;

public:
  void add(Buffer&& buffer) {
    auto found = buffers_.find(buffer.Type());
    if (found == buffers_.end()) {
      buffers_[buffer.Type()] = std::move(buffer);
    }
  }

  void add(ItemType type, size_t capacity) {
    auto found = buffers_.find(type);
    if (found == buffers_.end()) {
      buffers_[type] = Buffer{ capacity, type };
    }
  }

  // Pushes an item onto the matching buffer IFF the type already exists in
  // storage.
  size_t push(Item* item) {
    auto found = buffers_.find(item->type);
    if (found != buffers_.end()) {
      return buffers_[item->type].Push(item);
    }
    return 0;
  }

  // Pushes a buffer onto the matching buffer IFF the type already exists in
  // storage.
  size_t push(Buffer* buffer) {
    auto found = buffers_.find(buffer->Type());
    if (found != buffers_.end()) {
      return buffers_[buffer->Type()].Push(buffer);
    }
    return 0;
  }

  // Pulls the type and amount from the matching buffer.
  Item pull(ItemType type, size_t amount) {
    auto found = buffers_.find(type);
    if (found != buffers_.end()) {
      return buffers_[type].Pull(amount);
    }
    return Item{};
  }

  bool contains(ItemType type) {
    auto found = buffers_.find(type);
    return found != buffers_.end();
  }

  typename decltype(buffers_)::iterator find(ItemType type) {
    return buffers_.find(type);
  }

  typename decltype(buffers_)::const_iterator find(ItemType type) const {
    return buffers_.find(type);
  }

  typename decltype(buffers_)::iterator begin() {
    return buffers_.begin();
  }

  typename decltype(buffers_)::const_iterator begin() const {
    return buffers_.begin();
  }

  typename decltype(buffers_)::iterator end() {
    return buffers_.end();
  }

  typename decltype(buffers_)::const_iterator end() const {
    return buffers_.end();
  }
};

struct BuildingType {
  BuildingType(const std::string& name, ItemType type, glm::ivec2 size,
               GLuint img, Blueprint* blueprint) :
    name(name), type(type), size(size), img(img), blueprint(blueprint) {}

  const std::string name;
  const ItemType type;
  const glm::ivec2 size;
  const GLuint img;
  const Blueprint* const blueprint;
};

struct Mine {
  Item item;
};

struct Administration {
  Item energy;
};

template<class Component_>
struct ComponentList {
  ComponentList(const std::vector<Component_>& components) {
    list = new Component_[components.size()];
    size = components.size();
    for (size_t i = 0; i < size; ++i) {
      list[i] = components[i];
    }
  }

  Component_* list;
  size_t size;
};

class Building {
public:
  Building(BuildingType* type, qbEntity impl) : type_(type), impl_(impl) {}

  ~Building() {
    qb_entity_destroy(impl_);
  }

  BuildingType* Type() {
    return type_;
  }

  qbEntity Impl() {
    return impl_;
  }

  Storage* Input() {
    if (!qb_entity_hascomponent(impl_, input_storage_component)) {
      return nullptr;
    }

    Storage** storage_p;
    qb_instance_find(input_storage_component, impl_, &storage_p);
    return *storage_p;
  }

  Storage* Output() {
    if (!qb_entity_hascomponent(impl_, output_storage_component)) {
      return nullptr;
    }
    Storage** storage_p;
    qb_instance_find(output_storage_component, impl_, &storage_p);
    return *storage_p;
  }

private:
  BuildingType* type_;
  qbEntity impl_;
};

class PlanetaryGrid {
public:
  struct Cell {
    glm::ivec2 pos = { 0, 0 };

    Tile tile;
    uint8_t rotation = 0;

    std::shared_ptr<Building> building;

    bool empty = true;
  };

  PlanetaryGrid(size_t grid_size) : grid_size_(grid_size) {
    cells_.resize(grid_size);
    for (std::vector<Cell>& cells : cells_) {
      cells.resize(grid_size);
    }
  }

  Cell* Parent(glm::ivec2 p) {
    Cell* cell = At(p);
    if (cell) {
      if (cell->empty) {
        return cell;
      }
      return &cells_[cell->pos.y][cell->pos.x];
    }
    return nullptr;
  }

  Cell* At(glm::ivec2 p) {
    if (IsOutOfBounds(p)) {
      return nullptr;
    }

    return &cells_[p.y][p.x];
  }

  bool IsEmpty(glm::ivec2 p) const {
    if (IsOutOfBounds(p)) {
      return true;
    }
    return cells_[p.y][p.x].empty;
  }

  bool IsOccupied(glm::ivec2 p) const {
    if (IsOutOfBounds(p)) {
      return true;
    }
    return !IsEmpty(p);
  }

  bool CanBuild(glm::ivec2 pos, glm::ivec2 size) {
    if (IsOutOfBounds({ pos.x + size.x - 1, pos.y + size.y - 1 })) {
      return false;
    }

    for (uint8_t i = pos.x; i < pos.x + size.x; ++i) {
      for (uint8_t j = pos.y; j < pos.y + size.y; ++j) {
        if (IsOccupied(glm::ivec2(i, j))) {
          return false;
        }
      }
    }

    return true;
  }

  Cell* Build(Building* building, glm::ivec2 pos, glm::ivec2 size, uint8_t rotation) {
    if (IsOutOfBounds({ pos.x + size.x - 1, pos.y + size.y - 1 })) {
      return nullptr;
    }

    for (uint8_t i = pos.x; i < pos.x + size.x; ++i) {
      for (uint8_t j = pos.y; j < pos.y + size.y; ++j) {
        if (IsOccupied(glm::ivec2(i, j))) {
          return nullptr;
        }
      }
    }

    auto shared_building = std::shared_ptr<Building>(building);
    for (uint8_t i = pos.x; i < pos.x + size.x; ++i) {
      for (uint8_t j = pos.y; j < pos.y + size.y; ++j) {
        Cell* cell = At(glm::ivec2(i, j));
        cell->building = shared_building;
        cell->pos = pos;
        cell->rotation = rotation;
        cell->empty = false;
      }
    }

    return Parent(pos);
  }

  bool Destroy(glm::ivec2 p) {
    if (IsOutOfBounds(p)) {
      return false;
    }

    if (!IsOccupied(p)) {
      return true;
    }

    const Cell& parent = *Parent(p);
    glm::ivec2 pos = parent.pos;
    glm::ivec2 size = parent.building->Type()->size;

    for (uint8_t i = pos.x; i < pos.x + size.x; ++i) {
      for (uint8_t j = pos.y; j < pos.y + size.y; ++j) {
        Cell* cell = At(glm::ivec2(i, j));
        cell->building.reset();
        cell->pos = { 0, 0 };
        cell->empty = true;
      }
    }

    return true;
  }

  void Rotate(glm::ivec2 p, uint8_t rotation) {
    Cell* parent = Parent(p);
    if (!parent) {
      return;
    }

    parent->rotation = rotation;
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
          cell.building.reset();
          cell.pos = { -1, -1 };
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

  std::vector<std::pair<glm::ivec2, Tile>> GetTiles(glm::ivec2 topleft, glm::ivec2 bottomright) {
    std::vector<std::pair<glm::ivec2, Tile>> ret;
    for (uint8_t x = topleft.x; x < bottomright.x; ++x) {
      for (uint8_t y = topleft.y; y < bottomright.y; ++y) {
        Cell* cell = At(glm::ivec2(x, y));
        ret.emplace_back(glm::ivec2(x, y), cell->tile);
      }
    }
    return ret;
  }

private:
  bool IsOutOfBounds(glm::ivec2 p) const {
    return p.x >= (int)grid_size_ || p.y >= (int)grid_size_;
  }

  const size_t grid_size_;
  std::vector<std::vector<Cell>> cells_;
  int population_;
  int energy_;
};

class Producer {
public:
  enum class State {
    WAITING,
    PRODUCING,
  };

  Producer(Blueprint* bp) : blueprint_(bp),
    state_(State::WAITING), produce_trigger_(0) {}

  static void Produce(qbInstance* insts, qbFrame*) {
    Producer** producer_p;
    qb_instance_getconst(insts[0], &producer_p);

    CellRef* cell_ref;
    qb_instance_getconst(insts[1], &cell_ref);

    auto parent = cell_ref->grid->Parent(cell_ref->pos)->building;

    Producer* producer = *producer_p;
    Blueprint* bp = producer->blueprint_;

    // Transition to making the item if the storage has enough.
    if (producer->state_ == State::WAITING) {
      Storage* storage = parent->Input();
      if (storage) {
        bool has_enough_resources = true; // producer->energy_->amount >= bp->energy_cost;
        for (const Item& item : bp->requires) {
          auto found = storage->find(item.type);
          has_enough_resources &= found->second.Size() >= item.amount;
        }

        if (!has_enough_resources) {
          return;
        }

        for (const Item& item : bp->requires) {
          auto found = storage->find(item.type);
          found->second.Pull(item.amount);
        }
        producer->produce_trigger_ = 0;
        producer->state_ = State::PRODUCING;
      }
    }

    // Wait until we have made the item.
    if (producer->state_ == State::PRODUCING) {
      ++producer->produce_trigger_;
      if (producer->produce_trigger_ >= bp->tick_cost) {
        Storage* storage = parent->Output();
        if (storage) {
          for (const auto& product : bp->produces) {
            Item made = product;
            storage->push(&made);
          }

          producer->produce_trigger_ = 0;
          producer->state_ = State::WAITING;
        }
      }
    }
  }

private:
  // Input Storage
  qbEntity input_;

  // Output Storage
  qbEntity output_;

  Blueprint* blueprint_;
  Item* energy_;

  State state_;

  int produce_trigger_;
};

qbMaterial dot_material;
qbMesh dot_mesh;
render::qbRenderable dot_renderable;

std::unordered_map<std::string, Blueprint*> blueprints;
std::unordered_map<std::string, BuildingType*> buildings;
std::unordered_map<ItemType, GLuint> building_images;

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

void InitializeBuildings(const std::unordered_map<ItemType, GLuint>& images,
                         std::unordered_map<std::string, BuildingType*>* buildings) {
  (*buildings)["MINE"] = new BuildingType(
    "MINE",
    (ItemType)ProductionType::MINE,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)ProductionType::MINE),
    nullptr
  );

  (*buildings)["ORE PROCESSING"] = new BuildingType(
    "ORE PROCESSING",
    (ItemType)ProductionType::PROCESSOR,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)ProductionType::PROCESSOR),
    blueprints["METAL"]
  );

  (*buildings)["MINERAL PROCESSING"] = new BuildingType(
    "MINERAL PROCESSING",
    (ItemType)ProductionType::PROCESSOR,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)ProductionType::PROCESSOR),
    blueprints["CRYSTAL"]
  );

  (*buildings)["CITY"] = new BuildingType(
    "CITY",
    (ItemType)ProductionType::CITY,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)ProductionType::CITY),
    nullptr
  );

  (*buildings)["FARM"] = new BuildingType(
    "FARM",
    (ItemType)ProductionType::FARM,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)ProductionType::FARM),
    nullptr
  );


  (*buildings)["ACADEMY"] = new BuildingType(
    "ACADEMY",
    (ItemType)ProductionType::ACADEMY,
    glm::ivec2{ 4, 4 },
    images.at((ItemType)ProductionType::ACADEMY),
    nullptr
  );

  (*buildings)["ELECTRONICS FACTORY"] = new BuildingType(
    "ELECTRONICS FACTORY",
    (ItemType)ProductionType::FACTORY,
    glm::ivec2{ 3, 3 },
    images.at((ItemType)ProductionType::FACTORY),
    blueprints["ELECTRONICS"]
  );

  (*buildings)["SOLAR GENERATOR"] = new BuildingType(
    "SOLAR GENERATOR",
    (ItemType)ProductionType::SOLAR_PLANT,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)ProductionType::SOLAR_PLANT),
    blueprints["SOLAR_PLANT"]
  );

  (*buildings)["ROCKET SILO"] = new BuildingType(
    "ROCKET SILO",
    (ItemType)LogisticsType::ROCKET_SILO,
    glm::ivec2{ 4, 4 },
    images.at((ItemType)LogisticsType::ROCKET_SILO),
    nullptr
  );

  (*buildings)["TRANSPORTER"] = new BuildingType(
    "TRANSPORTER",
    (ItemType)LogisticsType::TRANSPORTER,
    glm::ivec2{ 1, 1 },
    images.at((ItemType)LogisticsType::TRANSPORTER),
    nullptr
  );

  (*buildings)["STORAGE"] = new BuildingType(
    "STORAGE",
    (ItemType)LogisticsType::STORAGE,
    glm::ivec2{ 1, 1 },
    images.at((ItemType)LogisticsType::STORAGE),
    nullptr
  );

  (*buildings)["INSERTER"] = new BuildingType(
    "INSERTER",
    (ItemType)LogisticsType::INSERTER,
    glm::ivec2{ 1, 1 },
    images.at((ItemType)LogisticsType::INSERTER),
    nullptr
  );

  (*buildings)["ADMINISTRATION"] = new BuildingType(
    "ADMINISTRATION",
    (ItemType)ProductionType::ADMINISTRATION,
    glm::ivec2{ 5, 5 },
    images.at((ItemType)ProductionType::ADMINISTRATION),
    nullptr
  );

  (*buildings)["DOCK"] = new BuildingType(
    "DOCK",
    (ItemType)LogisticsType::STATION_DOCK,
    glm::ivec2{ 2, 2 },
    images.at((ItemType)LogisticsType::STATION_DOCK),
    nullptr
  );
}

void InitializeBuildingImages(const Settings& settings, std::unordered_map<ItemType, GLuint>* images) {
  (*images)[(ItemType)ProductionType::MINE] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)ProductionType::PROCESSOR] = LoadImage(settings.resource_folder + "processing_facility.png");
  (*images)[(ItemType)ProductionType::CITY] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)ProductionType::FARM] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)ProductionType::ADMINISTRATION] = LoadImage(settings.resource_folder + "mine.png");

  (*images)[(ItemType)ProductionType::ACADEMY] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)ProductionType::FACTORY] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)ProductionType::SOLAR_PLANT] = LoadImage(settings.resource_folder + "mine.png");

  (*images)[(ItemType)LogisticsType::ROCKET_SILO] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)LogisticsType::TRANSPORTER] = LoadImage(settings.resource_folder + "transporter.png");
  (*images)[(ItemType)LogisticsType::STORAGE] = LoadImage(settings.resource_folder + "mine.png");
  (*images)[(ItemType)LogisticsType::INSERTER] = LoadImage(settings.resource_folder + "inserter.png");
  (*images)[(ItemType)LogisticsType::STATION_DOCK] = LoadImage(settings.resource_folder + "mine.png");
}

qbEntity CreateBuffer(Buffer buffer) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  qb_entityattr_addcomponent(attr, buffer_component, &buffer);

  qbEntity ret;
  qb_entity_create(&ret, attr);

  qb_entityattr_destroy(&attr);

  return ret;
}

qbEntity CreateTransporter(PlanetaryGrid* grid, glm::ivec2 pos, glm::ivec2 source, glm::ivec2 sink) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Transporter t;
  t.source = source;
  t.sink = sink;
  qb_entityattr_addcomponent(attr, transporter_component, &t);

  CellRef cell_ref;
  cell_ref.grid = grid;
  cell_ref.pos = pos;
  qb_entityattr_addcomponent(attr, cell_ref_component, &cell_ref);

  Buffer buffer(100000);
  qb_entityattr_addcomponent(attr, buffer_component, &buffer);

  qbEntity ret;
  qb_entity_create(&ret, attr);

  qb_entityattr_destroy(&attr);

  return ret;
}

Storage CreateStorage(std::set<Buffer> types) {
  Storage ret;
  for (Buffer buffer : types) {
    ret.add(std::move(buffer));
  }
  return ret;
}

Storage CreateStorage(std::set<Item> types) {
  Storage ret;
  for (Item item : types) {
    ret.add(item.type, item.amount);
  }
  return ret;
}

qbEntity CreateMine(PlanetaryGrid* grid, glm::ivec2 pos, Item item_to_mine) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Mine m;
  m.item = item_to_mine;
  qb_entityattr_addcomponent(attr, mine_component, &m);

  CellRef ref;
  ref.grid = grid;
  ref.pos = pos;
  qb_entityattr_addcomponent(attr, cell_ref_component, &ref);

  Storage* s = new Storage(CreateStorage({ Buffer(10000, item_to_mine.type) }));
  qb_entityattr_addcomponent(attr, output_storage_component, &s);

  qbEntity ret;
  qb_entity_create(&ret, attr);

  qb_entityattr_destroy(&attr);

  return ret;
}

qbEntity CreateProducer(PlanetaryGrid* grid, glm::ivec2 pos, const std::string& bp) {
  qbEntity ret = -1;
  Blueprint* blueprint = blueprints[bp];
  if (blueprint) {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    Producer* producer = (Producer*)malloc(sizeof(Producer));
    new(producer) Producer(blueprint);
    qb_entityattr_addcomponent(attr, producer_component, &producer);

    CellRef ref;
    ref.grid = grid;
    ref.pos = pos;
    qb_entityattr_addcomponent(attr, cell_ref_component, &ref);

    if (!blueprint->requires.empty()) {
      Storage* input = new Storage(CreateStorage(blueprint->requires));
      qb_entityattr_addcomponent(attr, input_storage_component, &input);
    }

    if (!blueprint->produces.empty()) {
      Storage* output = new Storage(CreateStorage(blueprint->produces));
      qb_entityattr_addcomponent(attr, output_storage_component, &output);
    }

    qb_entity_create(&ret, attr);

    qb_entityattr_destroy(&attr);
  }
  return ret;
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
    PlanetaryGrid* grid = new PlanetaryGrid(grid_builder.Build());
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

qbEntity CreateAdministration() {
  qbEntity ret;

  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Administration admin;
  qb_entityattr_addcomponent(attr, administration_component, &admin);
  ret = qb_entity_create(&ret, attr);

  qb_entityattr_destroy(&attr);
  return ret;
}

void Initialize(const Settings& settings) {
  InitializeBuildingImages(settings, &building_images);
  InitializeBlueprints(&blueprints);
  InitializeBuildings(building_images, &buildings);

  shader = settings.shader;
  InitializeComponent(&orbit_component, Orbit);
  InitializeComponent(&colorable_component, Colorable);
  InitializeComponent(&suicidal_component, Suicidal);
  InitializeComponent(&storage_component, Storage*);
  InitializeComponent(&input_storage_component, Storage*);
  InitializeComponent(&output_storage_component, Storage*);
  InitializeComponent(&producer_component, Producer*);
  InitializeComponent(&planetary_grid_component, PlanetaryGrid*);
  InitializeComponent(&buffer_component, Buffer);
  InitializeComponent(&transporter_component, Transporter);
  InitializeComponent(&mine_component, Mine);
  InitializeComponent(&item_component, Item);
  InitializeComponent(&cell_ref_component, CellRef);
  InitializeComponent(&administration_component, Administration);

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
    qb_systemattr_addconst(attr, cell_ref_component);
    qb_systemattr_addconst(attr, buffer_component);
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_INNER);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      Transporter* transport;
      qb_instance_getconst(insts[0], &transport);

      CellRef* cell_ref;
      qb_instance_getconst(insts[1], &cell_ref);

      Buffer* held;
      qb_instance_getconst(insts[2], &held);

      PlanetaryGrid* grid = cell_ref->grid;
      PlanetaryGrid::Cell* parent = grid->At(cell_ref->pos);
      glm::ivec2 src = cell_ref->pos + BuildingRotationToPoint(parent->rotation + 2);
      glm::ivec2 dst = cell_ref->pos + BuildingRotationToPoint(parent->rotation);

      // Move from what is currently held to the sink.
      if (!grid->IsEmpty(dst) && !held->Empty()) {
        auto& sink = grid->At(dst)->building;
        Storage* storage = sink->Input();
        if (storage) {
          auto found = storage->find(held->Peek()->type);
          if (found != storage->end()) {
            storage->push(held->Peek());
          }
        } else if ((LogisticsType)sink->Type()->type == LogisticsType::TRANSPORTER) {
          Buffer* other_held;
          qb_instance_find(buffer_component, sink->Impl(), &other_held);
          other_held->Push(held);
        }
      }

      // Move from source to what is held.
      if (!grid->IsEmpty(src)) {
        auto& source = grid->At(src)->building;

        Storage* storage = source->Output();
        if (storage) {
          size_t to_pull = 0;
          Item pulled;
          if (held->Empty()) {
            to_pull = held->Capacity();
            for (auto pair : *storage) {
              if (pair.second.Size() >= transport->pull_speed) {
                pulled = pair.second.Pull(transport->pull_speed);
              }
            }
          } else {
            auto found = storage->find(held->Peek()->type);
            if (found != storage->end()) {
              pulled = found->second.Pull(transport->pull_speed);
            }
          }
          held->Push(&pulled);
        } else if ((LogisticsType)source->Type()->type == LogisticsType::TRANSPORTER) {
          Buffer* other_held;
          qb_instance_find(buffer_component, source->Impl(), &other_held);
          held->Push(other_held);
        }
      }
    });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, mine_component);
    qb_systemattr_addconst(attr, cell_ref_component);
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_INNER);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      Mine* mine;
      qb_instance_getmutable(insts[0], &mine);
      
      CellRef* ref;
      qb_instance_getconst(insts[1], &ref);
      PlanetaryGrid* grid = ref->grid;
      PlanetaryGrid::Cell* parent = grid->At(ref->pos);

      for (auto tile : grid->GetTiles(ref->pos, parent->building->Type()->size)) {
        if ((ResourceType)tile.second.type == ResourceType::ORE ||
            (ResourceType)tile.second.type == ResourceType::MINERAL || 
            (ResourceType)tile.second.type == ResourceType::GAS) {

        }
      }

      Storage* storage = parent->building->Output();
      Item item(mine->item);
      storage->push(&item);
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
#if 0
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

        TradeShip ship;
        ship.payload = Item(trade_route->type, 1);
        ship.destination = trade_route->sink;

        if (found->amount > 0) {
          --found->amount;
        }

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
#endif
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
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, producer_component);
    qb_systemattr_addconst(attr, cell_ref_component);
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

    PlanetaryGrid* grid = new PlanetaryGrid(builder.Build());
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

  // Solar power stations.
  CreateStation(sun, 1, 50, 0.0005f, 90, glm::vec3{ 1, 1, 0 });
  CreateStation(sun, 1, 50, 0.0001f, 270, glm::vec3{ 1, 1, 0 });

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

    PlanetaryGrid* grid = new PlanetaryGrid(builder.Build());
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

    PlanetaryGrid* grid = new PlanetaryGrid(builder.Build());
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


  // Moon space station
  CreateStation(planet_entity, 0.05f, 0.125f, 2, 180, { 0.2, 0.2, 0.2 });

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

    PlanetaryGrid* grid = new PlanetaryGrid(builder.Build());
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
#if 0
BuildingType* SelectBuildingComboBoxWidget() {
  static BuildingType* selected_building = nullptr;

  struct nk_rect space;
  enum nk_widget_layout_states state;
  state = nk_widget(&space, gui::Context());
  nk_layout_row_dynamic(gui::Context(), 30, 1);
  nk_label(gui::Context(), "Buildings:", NK_TEXT_LEFT);
  if (nk_combo_begin_label(gui::Context(), selected_building ? selected_building->name.c_str() : "", nk_vec2(nk_widget_width(gui::Context()), 200))) {
    nk_layout_row_dynamic(gui::Context(), 25, 1);
    for (const auto& building_pair : buildings) {
      if (nk_combo_item_label(gui::Context(), building_pair.second->name.c_str(), NK_TEXT_LEFT)) {
        selected_building = building_pair.second;
      }
    }
    nk_combo_end(gui::Context());
  }
  return selected_building;
}

glm::ivec2 PlanetaryGridWidget(qbEntity selected, BuildingType* selected_building, int draw_size, bool* show_admin) {
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

  {
    static bool rotating = false;
    if (input::is_key_pressed(qbKey::QB_KEY_R) && !rotating) {
      rotating = true;
      rotation = (rotation + 1) % 4;
    }
    if (!input::is_key_pressed(qbKey::QB_KEY_R)) {
      rotating = false;
    }
  }

  for (size_t i = 0; i < grid_size * grid_size; ++i) {
    struct nk_rect space;
    enum nk_widget_layout_states state;
    state = nk_widget(&space, ctx);
    if (!state) return { -1, -1 };
    if (state != NK_WIDGET_ROM) {
      //update_your_widget_by_user_input(...);
    }

    PlanetaryGrid::Cell* cell = grid->At({ i % grid_size, i / grid_size });
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
      if (cell->empty) {
        nk_fill_rect(canvas, space, 0.0f, color);
      } else {
        glm::ivec2 pos = { i % grid_size, i / grid_size };
        PlanetaryGrid::Cell* parent = grid->Parent(pos);
        PlanetaryGrid::Cell* cell = grid->At(pos);
        struct nk_image* img = (struct nk_image*)&cell->building->Type()->img;
        img->w = parent->building->Type()->size.x;
        img->h = parent->building->Type()->size.y;
        img->region[0] = pos.x - parent->pos.x;
        img->region[1] = pos.y - parent->pos.y;
        img->region[2] = 1;
        img->region[3] = 1;

        nk_draw_rotated_image(canvas, space, parent->rotation * glm::half_pi<float>(), img, nk_rgb(255, 255, 255));
        //nk_draw_image(canvas, space, &transporter_img_u, nk_rgb(255, 255, 255));
        color = nk_rgb(255, 0, 255);
      }
    }

    if (state == NK_WIDGET_ROM) {
    } else if (state == NK_WIDGET_VALID) {
      color = nk_rgb(0, 255, 0);
      ret = { i % grid_size, i / grid_size };
      if (input::is_mouse_pressed(qbButton::QB_BUTTON_LEFT) && selected_building && grid->IsEmpty(ret) &&
          grid->CanBuild(ret, selected_building->size)) {
        qbEntity impl;
        if ((LogisticsType)selected_building->type == LogisticsType::TRANSPORTER) {
          glm::ivec2 src = ret + BuildingRotationToPoint(rotation);
          glm::ivec2 dst = ret + BuildingRotationToPoint(rotation + 2);
          impl = CreateTransporter(grid, ret, src, dst);
        } else if ((ProductionType)selected_building->type == ProductionType::MINE) {
          impl = -1;
        }

        Building* b = nullptr;
        if (selected_building->blueprint) {
          if (selected_building->name == "ORE PROCESSING") {
            b = new Building(selected_building, CreateProducer(grid, ret, "METAL"));
          } else if (selected_building->name == "MINERAL PROCESSING") {
            b = new Building(selected_building, CreateProducer(grid, ret, "CRYSTAL"));
          } else if ((ProductionType)selected_building->type == ProductionType::FACTORY) {
            b = new Building(selected_building, CreateProducer(grid, ret, "ELECTRONICS"));
          } else if ((ProductionType)selected_building->type == ProductionType::SOLAR_PLANT) {
            b = new Building(selected_building, CreateProducer(grid, ret, "SOLAR GENERATOR"));
          }
        } else {
          if ((LogisticsType)selected_building->type == LogisticsType::TRANSPORTER) {
            b = new Building(selected_building, impl);
          } else if ((ProductionType)selected_building->type == ProductionType::MINE) {
            auto tiles = grid->GetTiles(ret, ret + selected_building->size);
            size_t ore_count = 0;
            size_t mineral_count = 0;
            for (const auto& tile : tiles) {
              if (tile.second.resource.type == ResourceType::ORE) {
                ++ore_count;
              } else if (tile.second.resource.type == ResourceType::MINERAL) {
                ++mineral_count;
              }
            }

            Item item_to_mine(ore_count > mineral_count ? ResourceType::ORE : ResourceType::MINERAL, 10);
            qbEntity mine = CreateMine(grid, ret, item_to_mine);
            b = new Building(selected_building, mine);
          } else if ((ProductionType)selected_building->type == ProductionType::ADMINISTRATION) {
            b = new Building(selected_building, CreateAdministration());
          }
        }

        PlanetaryGrid::Cell* built = grid->Build(b,
                                                 ret,
                                                 selected_building->size,
                                                 rotation);
      } else if (input::is_mouse_pressed(qbButton::QB_BUTTON_LEFT) && !grid->IsEmpty(ret) &&
                 (ProductionType)grid->At(ret)->building->Type()->type == ProductionType::ADMINISTRATION) {
        *show_admin = true;
      }

      if (input::is_mouse_pressed(qbButton::QB_BUTTON_RIGHT)) {
        grid->Destroy(ret);
      }

      {
        static bool rotating = false;
        if (input::is_key_pressed(qbKey::QB_KEY_R) && !rotating) {
          rotating = true;
          PlanetaryGrid::Cell* parent = grid->Parent({ i % grid_size, i / grid_size });
          parent->rotation = (parent->rotation + 1) % 4;
        }
        if (!input::is_key_pressed(qbKey::QB_KEY_R)) {
          rotating = false;
        }
      }
    }
  }



  return ret;
}

void RenderPlanetPopup() {
  static bool show_admin = false;

  qbEntity selected = player::SelectedEntity();
  if (selected >= 0) {
    static BuildingType* selected_building = nullptr;
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
      PlanetaryGridWidget(selected, selected_building, 10, &show_admin);
      selected_building = SelectBuildingComboBoxWidget();
    }
    nk_end(gui::Context());
  }

  if (show_admin) {
    auto ctx = gui::Context();
    ctx->style.window.spacing = nk_vec2(0, 0);
    ctx->style.window.padding = nk_vec2(0, 0);
    //ctx->style.window.fixed_background = nk_style_item_color(nk_rgb(255, 255, 0));
    if (nk_begin(gui::Context(), "Planetary Administration", nk_rect(410, 10, 400, 600),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_DYNAMIC)) {

      nk_command_buffer* canvas = nk_window_get_canvas(gui::Context());
      {
        struct nk_rect total_space;
        total_space = nk_window_get_content_region(ctx);
        nk_layout_row_dynamic(ctx, total_space.h, 1);
        nk_widget(&total_space, ctx);
        auto bounds = nk_window_get_bounds(ctx);

        canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, nk_rect(bounds.x + 10, bounds.y + 10, 200, 600), 0, nk_rgb(255, 0, 255));
        nk_fill_rect(canvas, nk_rect(250, 20, 100, 100), 0, nk_rgb(0, 0, 255));
        nk_fill_circle(canvas, nk_rect(20, 250, 100, 100), nk_rgb(255, 0, 0));
      }

    } else {
      show_admin = false;
    }
    nk_end(gui::Context());
  }
}
#endif

}
