#include "items.h"

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