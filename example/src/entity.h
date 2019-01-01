#ifndef ENTITY__H
#define ENTITY__H

#include "inc/cubez.h"

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


#endif  // ENTITY__H