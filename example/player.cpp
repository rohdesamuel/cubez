#include "player.h"

#include "input.h"
#include "physics.h"
#include "render.h"
#include "shader.h"
#include "ball.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

namespace player {

// State
qbComponent players;
qbEntity main_player;
qbSystem on_key_event;

struct Player {
  bool fire_bullets;
};


void initialize(const Settings& settings) {
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Player);
    
    qb_component_create(&players, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    Player p;
    p.fire_bullets = false;

    physics::Transform t;
    t.p = settings.start_pos;
    t.v = {0, 0, 0};

    qb_entityattr_addcomponent(attr, players, &p);
    qb_entityattr_addcomponent(attr, physics::component(), &t);

    qb_entity_create(&main_player, attr);

    qb_entityattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, players);
    qb_systemattr_addsource(attr, physics::component());
    qb_systemattr_addsink(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr,
        [](qbElement* e, qbCollectionInterface*, qbFrame*) {
          physics::Transform t;
          qb_element_read(e[1], &t);

          glm::mat4 rot = render::qb_camera_getorientation();

          glm::vec3 impulse = glm::vec3(rot * glm::vec4{1.0f, 0.0f, 0.0f, 1.0f});
          glm::vec3 impulse_l = glm::vec3(rot * glm::vec4{0.0f, 1.0f, 0.0f, 1.0f});
          glm::vec3 impulse_r = glm::vec3(rot * glm::vec4{0.0f, -1.0f, 0.0f, 1.0f});

          impulse = glm::normalize(impulse);
          impulse_r = glm::normalize(impulse_r);
          impulse_l = glm::normalize(impulse_l);

          float floor_level = 8.0f;
          bool on_ground = t.p.z < floor_level + 0.01f;

          impulse *= on_ground ? 1.0f : 0.01f;
          impulse_l *= on_ground ? 1.0f : 0.01f;
          impulse_r *= on_ground ? 1.0f : 0.01f;

          if (input::is_key_pressed(QB_KEY_W)) {
            t.v += impulse;
          }
          if (input::is_key_pressed(QB_KEY_S)) {
            t.v -= impulse;
          }
          if (input::is_key_pressed(QB_KEY_A)) {
            t.v += impulse_l;
          }
          if (input::is_key_pressed(QB_KEY_D)) {
            t.v += impulse_r;
          }

          if (input::is_key_pressed(QB_KEY_I)) {
            render::qb_camera_incpitch(0.5f);
          }
          if (input::is_key_pressed(QB_KEY_K)) {
            render::qb_camera_incpitch(-0.5f);
          }
          if (input::is_key_pressed(QB_KEY_J)) {
            render::qb_camera_incyaw(0.5f);
          }
          if (input::is_key_pressed(QB_KEY_L)) {
            render::qb_camera_incyaw(-0.5f);
          }
          t.v.z -= 0.1f;

          if (on_ground) {
            t.p.z = floor_level;
            t.v.z = 0;
            t.v.x -= t.v.x * 0.25f;
            t.v.y -= t.v.y * 0.25f;
          }
          render::qb_camera_setposition(t.p);

          qb_element_write(e[1]);
        });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, players);
    qb_systemattr_addsource(attr, physics::component());
    qb_systemattr_addsink(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setfunction(attr,
        [](qbElement* els, qbCollectionInterface*, qbFrame* f) {
          input::InputEvent* e = (input::InputEvent*)f->event;
          if (e->key != QB_KEY_SPACE) {
            return;
          }

          physics::Transform t;
          qb_element_read(els[1], &t);

          if (!e->was_pressed && e->is_pressed && t.p.z <= 8.01f) {
            t.v.z += 5.0f;
          }

          qb_element_write(els[1]);
        });
    qbSystem unused;
    qb_system_create(&unused, attr);

    input::on_key_event(unused);

    qb_systemattr_destroy(&attr);
  }
  std::cout << "Finished initializing player\n";
}

}  // namespace player

