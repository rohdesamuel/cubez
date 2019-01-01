#ifndef ITEMS__H
#define ITEMS__H

#include <string>

enum class ItemType {
  NONE,
  ANY,
  RESOURCES = 0x100,
  INTERMEDIATE = 0x200,
  PRODUCTION = 0x300,
  LOGISTICS = 0x400,
  MILITARY = 0x500,
};

// Items that are requirements to manufacture all items.
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

// Items that are manufactured, but aren't directly used by the player.
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

// Items that can manufacture other items.
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

// Items that are used to move or store items.
enum class LogisticsType {
  STORAGE = (int)ItemType::LOGISTICS,
  INSERTER,
  TRANSPORTER,
  STATION_DOCK,
  ROCKET_SILO,

  // Auto-calculate the length of the enum.
  FINAL, LENGTH = (FINAL - (int)ItemType::LOGISTICS)
};

// Boom-booms.
enum class MilitaryType {
  // Auto-calculate the length of the enum.
  FINAL, LENGTH = (FINAL - (int)ItemType::MILITARY)
};

template<class EnumType>
constexpr ItemType GetItemType(EnumType value) {
  return (ItemType)((int)value & 0xFF00);
}

std::string ItemTypeToString(ItemType type);
ItemType StringToItemType(const std::string& type);

#endif  // ITEMS__H