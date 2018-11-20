#define __ENGINE_DEBUG__
#include <cubez/socket.h>

#include <chrono>
#include <iostream>
#include <memory.h>
#include <thread>

#ifdef __COMPILE_AS_WINDOWS__
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#endif

const size_t MAX_PACKET_SIZE = 65507;

#ifdef __COMPILE_AS_WINDOWS__
Socket::Socket(SOCKET socket, sockaddr_in receiver)
    : socket_(socket), receiver_(receiver) { }
#else
Socket::Socket(int socket, sockaddr_in receiver)
    : socket_(socket), receiver_(receiver) { }
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
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == -1) {
    perror("Socket::Create(address, port) error");
    return nullptr;
  }
  sockaddr_in receiver;
  memset(&receiver, 0, sizeof(sockaddr_in));
  receiver.sin_family = AF_INET;
  receiver.sin_port = htons(port);
  inet_pton(AF_INET, address, &receiver.sin_addr.s_addr);

  return std::make_unique<Socket>(sock, receiver);
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
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == -1) {
    perror("Socket::Create(port) error");
    return nullptr;
  }
  sockaddr_in receiver;
  memset(&receiver, 0, sizeof(sockaddr_in));
  receiver.sin_family = AF_INET;
  receiver.sin_port = htons(port);
  receiver.sin_addr.s_addr = INADDR_ANY;

  int result = bind(sock, (struct sockaddr*)&receiver, sizeof(receiver));
  if (result != 0) {
    perror("Socket::Create(port) error");
  }

  return std::make_unique<Socket>(sock, receiver);

#endif
}

Socket::~Socket() {
#ifdef __COMPILE_AS_WINDOWS__
  closesocket(socket_);
#else
#endif
}

void Socket::Bind(uint16_t port) {
	sockaddr_in receiver;
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(port);
	receiver.sin_addr.s_addr = htonl(INADDR_ANY);

	int result = bind(socket_, (SOCKADDR*)&receiver, sizeof(receiver));
	if (result != 0) {
		std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
	}
}

int Socket::Send(const char* buff, size_t length) {
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
  int result = sendto(socket_, buff, length, 0, (struct sockaddr*)(&receiver_),
                      sizeof(receiver_));
#ifdef __ENGINE_DEBUG__
  if (result == -1) {
    perror("Socket::Send(buff, length)");
  }
#endif
  return result;
#endif
}

int Socket::Send(const char* buff, size_t length, SockAddr* other_info) {
#ifdef __COMPILE_AS_WINDOWS__
  int result = sendto(socket_, buff, length, 0, (SOCKADDR*)(&other_info->addr),
					  other_info->socklen);
#ifdef __ENGINE_DEBUG__
  if (result == SOCKET_ERROR) {
    std::cout << "sendto failed with error: " << WSAGetLastError() << std::endl;
  }
#endif
  return result;
#else
  int result = sendto(socket_, buff, length, 0, (struct sockaddr*)(&receiver_),
                      sizeof(receiver_));
#ifdef __ENGINE_DEBUG__
  if (result == -1) {
    perror("Socket::Send(buff, length)");
  }
#endif
  return result;
#endif
}

int Socket::Receive(char* buff, size_t length, SockAddr* other_info) {
#ifdef __COMPILE_AS_WINDOWS__
  other_info->socklen = sizeof(SOCKADDR);
  int result = recvfrom(socket_, buff, length, 0, (SOCKADDR*)(&other_info->addr),
                        &other_info->socklen);
#ifdef __ENGINE_DEBUG__
  if (result == SOCKET_ERROR) {
    std::cout << "recvfrom failed with error: " << WSAGetLastError()
      << std::endl;
  }
#endif
  return result;
#else
  int result = recvfrom(socket_, buff, length, 0, (SOCKADDR*)(&other_info->addr),
                        sizeof(other_info->addr));
#ifdef __ENGINE_DEBUG__
  if (result == -1) {
    perror("Socket::Receive(buff, length)");
  }
#endif
  return result;
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
  int result = recvfrom(socket_, buff, length, 0, nullptr, nullptr);
#ifdef __ENGINE_DEBUG__
  if (result == -1) {
    perror("Socket::Receive(buff, length)");
  }
#endif
  return result;
#endif
}

std::unique_ptr<TestSocket>
TestSocket::Create(const char* address, uint16_t port, Options options) {
  auto socket = Socket::Create(address, port);
  return std::unique_ptr<TestSocket>(new TestSocket(std::move(socket), std::move(options)));
}

std::unique_ptr<TestSocket>
TestSocket::Create(uint16_t port, Options options) {
  auto socket = Socket::Create(port);
  return std::unique_ptr<TestSocket>(new TestSocket(std::move(socket), std::move(options)));
}

TestSocket::TestSocket(std::unique_ptr<Socket> socket, Options options)
  : opts_(std::move(options)),
    socket_(std::move(socket)) {}

int TestSocket::Send(const char* buff, size_t length) {
  if (rand() % precision_ > precision_ * opts_.packet_loss) {
    return socket_->Send(buff, length);
  }
  return length;
}

void TestSocket::Bind(uint16_t port) {
	socket_->Bind(port);
}

int TestSocket::Send(const char* buff, size_t length, SockAddr* other_info) {
  if (rand() % precision_ > precision_ * opts_.packet_loss) {
    return socket_->Send(buff, length, other_info);
  }
  return length;
}

int TestSocket::Receive(char* buff, size_t length, SockAddr* other_info) {
  std::this_thread::sleep_for(std::chrono::milliseconds(opts_.latency_ms));
  return socket_->Receive(buff, length, other_info);
}

int TestSocket::Receive(char* buff, size_t length) {
  std::this_thread::sleep_for(std::chrono::milliseconds(opts_.latency_ms));
  return socket_->Receive(buff, length);
}
