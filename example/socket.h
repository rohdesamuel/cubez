#ifndef SOCKET__H
#define SOCKET__H

#include <cubez/cubez.h>
#include <memory>

#ifdef __COMPILE_AS_WINDOWS__
#include <winsock2.h>
#else
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

  int Send(char* buff, size_t length);
  int Receive(char* buff, size_t length);

#ifdef __COMPILE_AS_WINDOWS__
  Socket(SOCKET sock, sockaddr_in receiver);
#else
#endif

private:
#ifdef __COMPILE_AS_WINDOWS__
  SOCKET socket_;
  sockaddr_in receiver_;
#else
#endif
};

#endif  // SOCKET__H