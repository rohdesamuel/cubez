#ifndef SOCKET__H
#define SOCKET__H

#include "common.h"
#include <memory>

#ifdef __COMPILE_AS_WINDOWS__
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

extern const size_t MAX_PACKET_SIZE;

class SocketInterface {
public:
  struct SockAddr {
#ifdef __COMPILE_AS_WINDOWS__
    SOCKADDR addr;
#else
    struct sockaddr addr;
#endif
    int socklen;
  };
  virtual void Bind(uint16_t port) = 0;

  virtual int Send(const char* buff, size_t length) = 0;
  virtual int Send(const char* buff, size_t length, SockAddr* other_info) = 0;
  virtual int Receive(char* buff, size_t length) = 0;
  virtual int Receive(char* buff, size_t length, SockAddr* other_info) = 0;
};

class DLLEXPORT Socket : public SocketInterface {
public:
  // Creates a UDP socket for sending data to the specified address and port.
  static std::unique_ptr<Socket> Create(const char* address, uint16_t port);

  // Creates a UDP socket for receiving data from a specific port.
  static std::unique_ptr<Socket> Create(uint16_t port);

  // Closes the socket.
  ~Socket();

  void Bind(uint16_t port) override;

  int Send(const char* buff, size_t length) override;
  int Send(const char* buff, size_t length, SockAddr* other_info) override;
  int Receive(char* buff, size_t length) override;
  int Receive(char* buff, size_t length, SockAddr* other_info) override;

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

class DLLEXPORT TestSocket : public SocketInterface {
public:
  struct Options {
    // Value in [0.0, 1.0]. 
    float packet_loss = 0.0;
    int latency_ms = 0;
  };

  // Creates a UDP socket for sending data to the specified address and port.
  static std::unique_ptr<TestSocket>
  Create(const char* address, uint16_t port, Options options);

  // Creates a UDP socket for receiving data from a specific port.
  static std::unique_ptr<TestSocket>
  Create(uint16_t port, Options options);

  void Bind(uint16_t port) override;

  int Send(const char* buff, size_t length) override;
  int Send(const char* buff, size_t length, SockAddr* other_info) override;
  int Receive(char* buff, size_t length) override;
  int Receive(char* buff, size_t length, SockAddr* other_info) override;

private:
  TestSocket(std::unique_ptr<Socket> socket, Options options);

  Options opts_;
  std::unique_ptr<Socket> socket_;

  const int precision_ = 10000;
};

#endif  // SOCKET__H
