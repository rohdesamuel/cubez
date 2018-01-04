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
        [](qbElement* e, qbFrame*) {
          Player player;
          qb_element_read(e[0], &player);

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

          if (player.fire_bullets) {
            glm::vec3 vel = glm::vec3(
                render::qb_camera_getorientation()
                * glm::vec4{5.0, 0.0, 0.0, 1.0});
            
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
            ball::create(t.p, vel, false);
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
        [](qbElement* els, qbFrame* f) {
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
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, players);
    qb_systemattr_addsink(attr, players);
    qb_systemattr_settrigger(attr, QB_TRIGGER_EVENT);
    qb_systemattr_setfunction(attr,
        [](qbElement* elements, qbFrame* f) {
          input::MouseEvent* e = (input::MouseEvent*)f->event;
          if (e->event_type == input::QB_MOUSE_EVENT_MOTION) {
            render::qb_camera_incyaw(-(float)(e->motion_event.xrel) * 0.25f);
            render::qb_camera_incpitch((float)(e->motion_event.yrel) * 0.25f);
          } else {
            Player p;
            qb_element_read(elements[0], &p);
            if (e->button_event.mouse_button == qbButton::QB_BUTTON_LEFT) {
              p.fire_bullets = e->button_event.state == 1;
            }
            qb_element_write(elements[0]);
          }
        });
    
    
    qbSystem mouse_movement;
    qb_system_create(&mouse_movement, attr);

    input::on_mouse_event(mouse_movement);

    qb_systemattr_destroy(&attr);
  }
  std::cout << "Finished initializing player\n";
}

}  // namespace player

