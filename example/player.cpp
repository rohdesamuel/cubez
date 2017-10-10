#include "player.h"

#include "input.h"
#include "physics.h"
#include "render.h"

namespace player {

struct Player {
  glm::vec3 color;
};

qbSystem key_press_handler;
qbComponent players;

void initialize(const Settings&) {
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setprogram(attr, kMainProgram);
    qb_componentattr_setdatatype(attr, Player);

    qb_component_create(&players, attr);

    qb_componentattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(attr, kMainProgram);
    qb_systemattr_addsource(attr, players);
    qb_systemattr_addsource(attr, physics::component());
    qb_systemattr_addsink(attr, physics::component());

    qb_system_create(&key_press_handler, attr);

    qb_systemattr_destroy(&attr);
  }
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entityattr_addcomponent(attr, players, nullptr);
    //qb_entityattr_addcomponent(attr, physics::component(), nullptr);
    //qb_entityattr_addcomponent(attr, render::component(), nullptr);

    qbEntity dummy;
    qb_entity_create(&dummy, attr);

    qb_entityattr_destroy(&attr);
  }
  input::on_key_press(key_press_handler);
}

void on_key_press(qbElement* elements,
                  qbCollectionInterface* collections, qbFrame* frame) {
  physics::Transform* transform = (physics::Transform*)elements[1].value;
  input::InputEvent* event = (input::InputEvent*)frame->event;

  if (event->alphanum == 'w') {
    transform->v += 1.0f;
  }

  collections->update(collections, &elements[1]);
}

}  // namespace player

