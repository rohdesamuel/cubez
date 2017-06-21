#include "player.h"

#include "physics.h"

namespace player {

// Event names
const char kMove[] = "player_insert";

// Collections
Object* player;

// Channels
qbChannel* move_channel;

// Systems
qbSystem* move_system;

// State
qbId physics_id;

void initialize(const Settings& settings) {
  player = new Object;
  player->color = settings.color;
  
  physics_id = physics::create(settings.start_pos, glm::vec3{});
}

void move_left(float speed) {
  physics::send_impulse(physics_id, {-speed, 0.0f, 0.0f});
}

void move_right(float speed) {
  physics::send_impulse(physics_id, {speed, 0.0f, 0.0f});
}

void move_up(float speed) {
  physics::send_impulse(physics_id, {0.0f, speed, 0.0f});
}

void move_down(float speed) {
  physics::send_impulse(physics_id, {0.0f, -speed, 0.0f});
}

}  // namespace player
