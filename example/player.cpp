#include "player.h"

#include "input.h"
#include "physics.h"
#include "render.h"
#include "shader.h"
#include "ball.h"
#include "mesh_builder.h"
#include "collision_utils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include "gui.h"

namespace player {

// State
qbComponent players;
qbEntity main_player;
qbSystem on_key_event;

Mesh* collision_mesh;

struct Player {
  bool fire_bullets;
  bool place_block;
};

qbComponent Component() {
  return players;
}

float dir = 0.0f;
float zdir = 45.0f;
float dis = 100.0f;
qbEntity selected = -1;
std::pair<qbEntity, qbEntity> selected_with_mouse_drag;

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
    t.is_fixed = false;

    MeshBuilder builder;
    builder.AddVertex({ -2, -2, 0 });
    builder.AddVertex({ -2,  2, 0 });
    builder.AddVertex({  2, -2, 0 });
    builder.AddVertex({  2,  2, 0 });
    builder.AddVertex({ -2, -2, 8 });
    builder.AddVertex({ -2,  2, 8 });
    builder.AddVertex({  2, -2, 8 });
    builder.AddVertex({  2,  2, 8 });
    collision_mesh = new Mesh(std::move(builder.BuildMesh()));

    physics::qbCollidable collidable;
    collidable.collision_mesh = collision_mesh;

    qb_entityattr_addcomponent(attr, players, &p);
    qb_entityattr_addcomponent(attr, physics::component(), &t);
    //qb_entityattr_addcomponent(attr, physics::collidable(), &collidable);

    qb_entity_create(&main_player, attr);

    qb_entityattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, players);
    qb_systemattr_addmutable(attr, physics::component());

    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame*) {
          Player* player;
          qb_instance_getconst(insts[0], &player);

          physics::Transform* t;
          qb_instance_getmutable(insts[1], &t);

          if (SDL_GetRelativeMouseMode()) {
            float speed = 1.0f;
            glm::vec3 origin = render::qb_camera_getorigin();
            if (input::is_key_pressed(QB_KEY_W)) {
              origin.x -= speed * glm::cos(glm::radians(dir));
              origin.y -= speed * glm::sin(glm::radians(dir));
            }
            if (input::is_key_pressed(QB_KEY_S)) {
              origin.x += speed * glm::cos(glm::radians(dir));
              origin.y += speed * glm::sin(glm::radians(dir));
            }
            if (input::is_key_pressed(QB_KEY_A)) {
              origin.x += speed * glm::cos(glm::radians(dir - 90));
              origin.y += speed * glm::sin(glm::radians(dir - 90));
            }
            if (input::is_key_pressed(QB_KEY_D)) {
              origin.x += speed * glm::cos(glm::radians(dir + 90));
              origin.y += speed * glm::sin(glm::radians(dir + 90));
            }
            render::qb_camera_setorigin(origin);
          }
          t->p.x = dis * glm::sin(glm::radians(zdir)) * glm::cos(glm::radians(dir));
          t->p.y = dis * glm::sin(glm::radians(zdir)) * glm::sin(glm::radians(dir));
          t->p.z = dis * glm::cos(glm::radians(zdir));
          render::qb_camera_setposition(t->p);
        });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, physics::collidable());
    qb_systemattr_addconst(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setcondition(attr, [](qbFrame* f) {
      input::MouseEvent* e = (input::MouseEvent*)f->event;
      bool should_run = e->event_type == input::qbMouseEvent::QB_MOUSE_EVENT_BUTTON && !nk_window_is_any_hovered(gui::Context());
      if (should_run) {
        selected = -1;
      }
      return should_run;
    });
    qb_systemattr_setfunction(attr,
                              [](qbInstance* insts, qbFrame* f) {
      input::MouseEvent* mouse_event = (input::MouseEvent*)f->event;
      input::MouseButtonEvent* e = &mouse_event->button_event;
      
      physics::qbCollidable* c;
      qb_instance_getconst(insts[0], &c);

      physics::Transform* t;
      qb_instance_getconst(insts[1], &t);

      glm::vec3 orig = render::qb_camera_getposition() + render::qb_camera_getorigin();
      int x, y;
      input::get_mouse_position(&x, &y);

      glm::vec3 dir;
      render::qb_camera_screentoworld({ x, y }, &dir);
      dir = glm::normalize(dir);
      Ray ray(std::move(orig), std::move(dir));

      if (collision_utils::CollidesWith(t->p, *c->collision_mesh, ray)) {
        qbEntity new_selected = qb_instance_getentity(insts[0]);
        if (e->mouse_button == qbButton::QB_BUTTON_LEFT) {
          selected = new_selected;
        }
        if (e->mouse_button == qbButton::QB_BUTTON_LEFT) {
          if (e->state == input::qbMouseState::QB_MOUSE_DOWN) {
            selected_with_mouse_drag.first = new_selected;
          } else {
            selected_with_mouse_drag.second = new_selected;
          }
        }
        if (input::is_key_pressed(qbKey::QB_KEY_LALT)) {
          physics::Transform* other;
          qb_instance_find(physics::component(), selected, &other);
          render::qb_camera_setorigin(other->p);
        }
      }
    });

    qbSystem selection;
    qb_system_create(&selection, attr);
    qb_systemattr_destroy(&attr);

    input::on_mouse_event(selection);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, players);
    qb_systemattr_addmutable(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame* f) {
          input::InputEvent* e = (input::InputEvent*)f->event;
          if (e->key != QB_KEY_SPACE) {
            return;
          }

          physics::Transform* t;
          qb_instance_getmutable(insts[1], &t);

          if (e->key == QB_KEY_SPACE) {// && !e->was_pressed && e->is_pressed && t->p.z <= 8.01f) {
            //t->v.z += 2.0f;
          }
        });
    qbSystem unused;
    qb_system_create(&unused, attr);

    input::on_key_event(unused);

    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, players);
    qb_systemattr_settrigger(attr, QB_TRIGGER_EVENT);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame* f) {
          input::MouseEvent* e = (input::MouseEvent*)f->event;
          if (e->event_type == input::QB_MOUSE_EVENT_MOTION) {
            dir += (-(float)(e->motion_event.xrel) * 0.25f);
            zdir += ((float)(e->motion_event.yrel) * 0.25f);
            zdir = std::max(std::min(zdir, 90.0f), 1.0f);
          } else {
            Player* p;
            qb_instance_getmutable(insts[0], &p);
            if (e->button_event.mouse_button == qbButton::QB_BUTTON_LEFT) {
              //p->fire_bullets = e->button_event.state == 1;
            } else if (e->button_event.mouse_button == qbButton::QB_BUTTON_RIGHT) {
              //p->place_block = e->button_event.state == 1;
            }
          }
        });
    
    
    qbSystem mouse_movement;
    qb_system_create(&mouse_movement, attr);

    input::on_mouse_event(mouse_movement);

    qb_systemattr_destroy(&attr);
  }
  std::cout << "Finished initializing player\n";
}

qbEntity SelectedEntity() {
  return selected;
}

std::pair<qbEntity, qbEntity> SelectedEntityMouseDrag() {
  return selected_with_mouse_drag;
}

}  // namespace player

