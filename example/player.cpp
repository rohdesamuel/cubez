#include "player.h"

#include "physics.h"

using cubez::Channel;
using cubez::Pipeline;
using cubez::send_message;
using cubez::open_channel;

namespace player {

// Event names
const char kMove[] = "player_insert";

// Collections
Object* player;

// Channels
Channel* move_channel;

// Pipelines
Pipeline* move_pipeline;

// State
cubez::Id physics_id;

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
