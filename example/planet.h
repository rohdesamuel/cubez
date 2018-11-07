#ifndef PLANET__H
#define PLANET__H

#include "inc/cubez.h"
#include "items.h"
#include "mesh.h"

namespace planet {

struct Settings {
  qbShader shader;
  std::string resource_folder;
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