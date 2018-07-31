#ifndef PLANET__H
#define PLANET__H

#include "inc/cubez.h"
#include "mesh.h"

namespace planet {

struct Settings {
  qbShader shader;
  std::string resource_folder;
};

enum class ItemType {
  NONE,
  RESOURCES = 0x100,
  INTERMEDIATE = 0x200,
  PRODUCTION = 0x300,
  LOGISTICS = 0x400
};

enum class ResourceType {
  ORE = (int)ItemType::RESOURCES, // Processed to Metal
  MINERAL,  // Processed to Crystal
  GAS,  // Processed to Polymer
  WATER,
  FOOD,
  ENERGY,
  POPULATION,
  HAPPINESS,

  // Auto-calculate the length of the enum.
  FINAL, LENGTH = (FINAL - (int)ItemType::RESOURCES)
};

enum class IntermediateType {
  METAL = (int)ItemType::INTERMEDIATE,
  CRYSTAL,
  POLYMER,
  CONSUMER_GOODS,
  SCIENCE,
  ELECTRONICS,

  // Auto-calculate the length of the enum.
  FINAL, LENGTH = (FINAL - (int)ItemType::INTERMEDIATE)
};

enum class ProductionType {
  CITY = (int)ItemType::PRODUCTION,
  ENTERTAINMENT,
  FARM,
  MINE,
  PROCESSOR,
  FACTORY,
  ACADEMY,
  ADMINISTRATION,
  SOLAR_PLANT,

  // Auto-calculate the length of the enum.
  FINAL, LENGTH = (FINAL - (int)ItemType::PRODUCTION)
};

enum class LogisticsType {
  STORAGE = (int)ItemType::LOGISTICS,
  INSERTER,
  TRANSPORTER,
  STATION_DOCK,
  ROCKET_SILO,

  // Auto-calculate the length of the enum.
  FINAL, LENGTH = (FINAL - (int)ItemType::LOGISTICS)
};

void Initialize(const Settings& settings);

std::string Description(qbEntity entity);
qbEntity CreateTradeRoute(qbEntity source, qbEntity sink, ItemType type);
qbEntity CreateProducer(qbEntity storage, ProductionType type);

std::string ItemTypeToString(ItemType type);
ItemType StringToItemType(const std::string& type);

void CreateProducerWidget();
void UICreateTradeRoute();
void RenderPlanetPopup();
}

#endif  // PLANET__H