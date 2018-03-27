#include "inc/cubez.h"
#include "inc/timer.h"

#include "ball.h"
#include "physics.h"
#include "input.h"
#include "player.h"
#include "log.h"
#include "render.h"
#include "network.h"
#include "socket.h"
#include "xdr.h"

#include <algorithm>
#include <iostream>
#include <thread>
#include <unordered_map>


void initialize_universe(qbUniverse* uni) {
  qb_init(uni);

  {
    logging::initialize();
  }

  {
    network::initialize();
  }

  {
    physics::Settings settings;
    physics::initialize(settings);
  }
}

int main(int, char*[]) {
  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);

  auto receiver = Socket::Create(25001);
  auto sender = Socket::Create("127.0.0.1", 25000);
  while (1) {
    char recv_buf[128] = { '\0' };
    std::cout << "received " << receiver->Receive(recv_buf, 128)
              << " bytes with message: " << recv_buf << std::endl;
    std::cout << "sending ack\n";

    uint8_t send_buf[128] = { '\0' };
    network::pack::Int64(send_buf, 1337);
    sender->Send("!", 1);
  }

  qb_start();
  int frame = 0;
  WindowTimer fps_timer(50);

  double start_time = Timer::now();
  qb_loop();
  while (1) {
    fps_timer.start();

    qb_loop();
    ++frame;

    fps_timer.stop();
    fps_timer.step();

    double time = Timer::now();

    static int prev_trigger = 0;
    static int trigger = 0;
    int period = 1;

    prev_trigger = trigger;
    trigger = int64_t(time - start_time) / 1000000000;

    if (trigger % period == 0 && prev_trigger != trigger) {
    //if (true && period && prev_trigger == prev_trigger && trigger == trigger) {
      //logging::out(
      std::cout <<
          "Frame " + std::to_string(frame) + "\n" +
          + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n";
    }
  }
}
