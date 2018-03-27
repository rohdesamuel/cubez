#ifndef NETWORK__H
#define NETWORK__H

namespace network {

void initialize();
void shutdown();

enum class qbServerCommand {
  CONNECT = 0,
  QUIT = 1,
};

}  // network

#endif  // NETWORK__H
