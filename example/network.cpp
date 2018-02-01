#include "network.h"

#include "inc/common.h"

#ifdef __COMPILE_AS_WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#endif

#include <iostream>

WSADATA wsa_data;

namespace network {

void initialize() {
#ifdef __COMPILE_AS_WINDOWS__
  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != NO_ERROR) {
    std::cout << "WSAStartup failed with error:" << result << std::endl;
    return;
  }
#else
#endif
}

void shutdown() {
#ifdef __COMPILE_AS_WINDOWS__
  WSACleanup();
#else
#endif
}

}  // network