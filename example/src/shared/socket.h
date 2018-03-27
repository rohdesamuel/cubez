#ifndef SOCKET__H
#define SOCKET__H

#include "inc/common.h"
#include <memory>

#ifdef __COMPILE_AS_WINDOWS__
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

extern const size_t MAX_PACKET_SIZE;

class Socket {
public:
  // Creates a UDP socket for sending data to the specified address and port.
  static std::unique_ptr<Socket> Create(const char* address, uint16_t port);

  // Creates a UDP socket for receiving data from a specific port.
  static std::unique_ptr<Socket> Create(uint16_t port);

  // Closes the socket.
  ~Socket();

  int Send(const char* buff, size_t length);
  int Receive(char* buff, size_t length);

#ifdef __COMPILE_AS_WINDOWS__
  Socket(SOCKET sock, sockaddr_in receiver);
#else
  Socket(int sock, sockaddr_in receiver);
#endif

private:
#ifdef __COMPILE_AS_WINDOWS__
  SOCKET socket_;
  sockaddr_in receiver_;
#else
  int socket_;
  sockaddr_in receiver_;
#endif
};

#endif  // SOCKET__H
