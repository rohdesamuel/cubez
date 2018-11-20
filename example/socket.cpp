#include "socket.h"

#include <iostream>

#ifdef __COMPILE_AS_WINDOWS__
#include <Ws2tcpip.h>
#else
#endif

const size_t MAX_PACKET_SIZE = 65507;

#ifdef __COMPILE_AS_WINDOWS__
Socket::Socket(SOCKET socket, sockaddr_in receiver)
    : socket_(socket), receiver_(receiver) { }
#else
#endif

/** static */
std::unique_ptr<Socket> Socket::Create(const char* address, uint16_t port) {
#ifdef __COMPILE_AS_WINDOWS__
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    std::cout << "Socket failed with error: " << WSAGetLastError();
    return nullptr;
  }
  sockaddr_in receiver;
  receiver.sin_family = AF_INET;
  receiver.sin_port = htons(port);
  InetPton(AF_INET, address, &receiver.sin_addr.s_addr);

  return std::make_unique<Socket>(sock, receiver);
#else
#endif
}

/** static */
std::unique_ptr<Socket> Socket::Create(uint16_t port) {
#ifdef __COMPILE_AS_WINDOWS__
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    std::cout << "Socket failed with error: " << WSAGetLastError();
    return nullptr;
  }
  sockaddr_in receiver;
  receiver.sin_family = AF_INET;
  receiver.sin_port = htons(port);
  receiver.sin_addr.s_addr = htonl(INADDR_ANY);

  int result = bind(sock, (SOCKADDR*)&receiver, sizeof(receiver));
  if (result != 0) {
    std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
  }

  return std::make_unique<Socket>(sock, receiver);
#else
#endif
}

Socket::~Socket() {
#ifdef __COMPILE_AS_WINDOWS__
  closesocket(socket_);
#else
#endif
}

int Socket::Send(char* buff, size_t length) {
#ifdef __COMPILE_AS_WINDOWS__
  int result = sendto(socket_, buff, length, 0, (SOCKADDR*)(&receiver_),
                      sizeof(receiver_));
#ifdef __ENGINE_DEBUG__
  if (result == SOCKET_ERROR) {
    std::cout << "sendto failed with error: " << WSAGetLastError() << std::endl;
  }
#endif
  return result;
#else
#endif
}

int Socket::Receive(char* buff, size_t length) {
#ifdef __COMPILE_AS_WINDOWS__
  int result = recvfrom(socket_, buff, length, 0, nullptr, nullptr);
#ifdef __ENGINE_DEBUG__
  if (result == SOCKET_ERROR) {
    std::cout << "recvfrom failed with error: " << WSAGetLastError()
              << std::endl;
  }
#endif
  return result;
#else
#endif
}