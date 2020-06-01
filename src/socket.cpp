/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <cubez/socket.h>
#include <cubez/common.h>

#ifdef __COMPILE_AS_WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <iostream>

// https://docs.microsoft.com/en-us/windows/win32/winsock/porting-socket-applications-to-winsock
// https://www.geeksforgeeks.org/socket-programming-cc/

#define QB_RETURN_SOCKET_ERROR                          \
do {                                                    \
  int err = socket_errno();                             \
  qbSocketError s_err = translate_qb_socket_error(err); \
  if (s_err == QB_SOCKET_UNKNOWN) {                     \
      errno = err;                                      \
      return QB_UNKNOWN;                                \
  } else {                                              \
      errno = s_err;                                    \
      return QB_ERROR_SOCKET;                           \
  }                                                     \
} while(0) 

#define QB_RETURN_SOCKET_MSG_ERROR                      \
do {                                                    \
  int err = socket_errno();                             \
  qbSocketError s_err = translate_qb_socket_error(err); \
  if (s_err == QB_SOCKET_UNKNOWN) {                     \
      errno = err;                                      \
      return -1;                                        \
  } else {                                              \
      errno = s_err;                                    \
      return -1;                                        \
  }                                                     \
} while(0) 

int translate_qb_af(qbAddressFamily af) {
  switch (af) {
    case QB_IPV4: return AF_INET;
    case QB_IPV6: return AF_INET6;
  }
  return -1;
}

int translate_qb_np(qbNetworkProtocol np) {
  switch (np) {
    case QB_UDP: return IPPROTO_UDP;
    case QB_TCP: return IPPROTO_TCP;
  }
  return -1;
}

int translate_qb_type(qbNetworkProtocol np) {
  switch (np) {
    case QB_UDP: return SOCK_DGRAM;
    case QB_TCP: return SOCK_STREAM;
  }
  return -1;
}

void ipv6_to_n(uint32_t* host_ipv6, uint32_t* net_ipv6) {
  net_ipv6[0] = htonl(host_ipv6[0]);
  net_ipv6[1] = htonl(host_ipv6[1]);
  net_ipv6[2] = htonl(host_ipv6[2]);
  net_ipv6[3] = htonl(host_ipv6[3]);
}

void ipv6_to_h(uint32_t* host_ipv6, uint32_t* net_ipv6) {
  host_ipv6[0] = ntohl(net_ipv6[0]);
  host_ipv6[1] = ntohl(net_ipv6[1]);
  host_ipv6[2] = ntohl(net_ipv6[2]);
  host_ipv6[3] = ntohl(net_ipv6[3]);
}

int32_t socket_errno() {
  int err = 0;
#ifdef __COMPILE_AS_WINDOWS__ 
  err = WSAGetLastError();
#else
  err = errno;
#endif
  return err;
} 

qbSocketError translate_qb_socket_error(int err) {
#ifdef __COMPILE_AS_WINDOWS__
  switch (err) {
    case WSANOTINITIALISED: FATAL("Trying to create a socket when network module not initialized.");
    case WSAENETDOWN: FATAL("Encountered a fatal error when trying to create a socket. "
                            "This could be indicative of a serious failure of the network system, "
                            "the network interface, or the local network itself.");
    case WSAEINVALIDPROVIDER: FATAL("Service provider returned a version number other than 2.2.");
    case WSAEINVALIDPROCTABLE: FATAL("The service provider returned an invalid or incomplete "
                                     "procedure table to the WSPStartup.");
    case WSAEPROVIDERFAILEDINIT: FATAL("The service provider failed to initialize. This error is "
                                       "returned if a layered service provider (LSP) or "
                                       "namespace provider was improperly installed or the "
                                       "provider fails to operate correctly.");
    case WSAEACCES: return QB_SOCKET_EACCES;
    case WSAEADDRINUSE: return QB_SOCKET_EADDRINUSE;
    case WSAEADDRNOTAVAIL: return QB_SOCKET_EADDRNOTAVAIL;
    case WSAEAFNOSUPPORT: return QB_SOCKET_EAFNOSUPPORT;
    case WSAEALREADY: return QB_SOCKET_EALREADY;
    case WSAECONNABORTED: return QB_SOCKET_ECONNABORTED;
    case WSAECONNREFUSED: return QB_SOCKET_ECONNREFUSED;
    case WSAECONNRESET: return QB_SOCKET_ECONNRESET;
    case WSAEDESTADDRREQ: return QB_SOCKET_EDESTADDRREQ;
    case WSAEFAULT: return QB_SOCKET_EFAULT;
    case WSAEHOSTUNREACH: return QB_SOCKET_EHOSTUNREACH;
    case WSAEINPROGRESS: return QB_SOCKET_EINPROGRESS;
    case WSAEINTR: return QB_SOCKET_EINTR;
    case WSAEINVAL: return QB_SOCKET_EINVAL;
    case WSAEISCONN: return QB_SOCKET_EISCONN;
    case WSAEMFILE: return QB_SOCKET_EMFILE;
    case WSAEMSGSIZE: return QB_SOCKET_EMSGSIZE;
    case WSAENETUNREACH: return QB_SOCKET_ENETUNREACH;
    case WSAENOBUFS: return QB_SOCKET_ENOBUFS;
    case WSAENOPROTOOPT: return QB_SOCKET_ENOPROTOOPT;
    case WSAENOTCONN: return QB_SOCKET_ENOTCONN;
    case WSAENOTSOCK: return QB_SOCKET_ENOTSOCK;
    case WSAEOPNOTSUPP: return QB_SOCKET_EOPNOTSUPP;
    case WSAEPROTONOSUPPORT: return QB_SOCKET_EPROTONOSUPPORT;
    case WSAEPROTOTYPE: return QB_SOCKET_EPROTOTYPE;
    case WSAESOCKTNOSUPPORT: return QB_SOCKET_ESOCKTNOSUPPORT;
    case WSAETIMEDOUT: return QB_SOCKET_ETIMEDOUT;
    case WSAEWOULDBLOCK: return QB_SOCKET_EWOULDBLOCK;
    case WSAESHUTDOWN: return QB_SOCKET_ESHUTDOWN;
    case WSAEHOSTDOWN: return  QB_SOCKET_EHOSTDOWN;
  }
#else
  switch (err) {
    case ENFILE: FATAL("The system-wide limit on the total number of open "
                       "files has been reached.");
    case EACCES: return QB_SOCKET_EACCES;
    case EADDRINUSE: return QB_SOCKET_EADDRINUSE;
    case EADDRNOTAVAIL: return QB_SOCKET_EADDRNOTAVAIL;
    case EAFNOSUPPORT: return QB_SOCKET_EAFNOSUPPORT;
    case EALREADY: return QB_SOCKET_EALREADY;
    case ECONNABORTED: return QB_SOCKET_ECONNABORTED;
    case ECONNREFUSED: return QB_SOCKET_ECONNREFUSED;
    case ECONNRESET: return QB_SOCKET_ECONNRESET;
    case EDESTADDRREQ: return QB_SOCKET_EDESTADDRREQ;
    case EFAULT: return QB_SOCKET_EFAULT;
    case EHOSTUNREACH: return QB_SOCKET_EHOSTUNREACH;
    case EINPROGRESS: return QB_SOCKET_EINPROGRESS;
    case EINTR: return QB_SOCKET_EINTR;
    case EINVAL: return QB_SOCKET_EINVAL;
    case EISCONN: return QB_SOCKET_EISCONN;
    case EMFILE: return QB_SOCKET_EMFILE;
    case EMSGSIZE: return QB_SOCKET_EMSGSIZE;
    case ENETUNREACH: return QB_SOCKET_ENETUNREACH;
    case ENOMEM:  // Fall-through intended.
    case ENOBUFS: return QB_SOCKET_ENOBUFS;
    case ENOPROTOOPT: return QB_SOCKET_ENOPROTOOPT;
    case EPIPE:  // Fall-through intended.
    case ENOTCONN: return QB_SOCKET_ENOTCONN;
    case EBADF:  // Fall-through intended.
    case ENOTSOCK: return QB_SOCKET_ENOTSOCK;
    case EOPNOTSUPP: return QB_SOCKET_EOPNOTSUPP;
    case EPROTONOSUPPORT: return QB_SOCKET_EPROTONOSUPPORT;
    case EPROTOTYPE: return QB_SOCKET_EPROTOTYPE;
    case ESOCKTNOSUPPORT: return QB_SOCKET_ESOCKTNOSUPPORT;
    case ETIMEDOUT: return QB_SOCKET_ETIMEDOUT;
    case EAGAIN:  // Fall-through intended.
    case EWOULDBLOCK: return QB_SOCKET_EWOULDBLOCK;
    case ESHUTDOWN: return QB_SOCKET_ESHUTDOWN;
    case EHOSTDOWN: return  QB_SOCKET_EHOSTDOWN;
  }
#endif  // __COMPILE_AS_WINDOWS__
  return QB_SOCKET_UNKNOWN;
}

qbResult qb_socket_create(qbSocket* socket_ref, qbSocketAttr attr) {
  int np = translate_qb_np(attr->np);
  int type = translate_qb_type(attr->np);
  int af = translate_qb_af(attr->af);

  qbSocket sock = socket(af, type, np);
#ifdef __COMPILE_AS_WINDOWS__
  if (sock == INVALID_SOCKET) {
    QB_RETURN_SOCKET_ERROR;
  }  
#else
  if (sock < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif

  *socket_ref = sock;
  return QB_OK;
}

qbResult qb_socket_close(qbSocket socket) {
#ifdef __COMPILE_AS_WINDOWS__  
  if (closesocket(socket) == SOCKET_ERROR) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (close(socket->socket_) < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

qbResult qb_socket_shutdown(qbSocket socket, qbSocketShutdown how) {
  int res = shutdown(socket, how);
#ifdef __COMPILE_AS_WINDOWS__  
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

qbResult qb_socket_bind(qbSocket socket, qbEndpoint endpoint) {
  int res = 0; EACCES;
  if (endpoint->af == QB_IPV4) {
    sockaddr_in receiver = {};
    receiver.sin_family = AF_INET;
    receiver.sin_port = endpoint->port;
    receiver.sin_addr.s_addr = endpoint->in_addr;
    res = bind(socket, (SOCKADDR*)&receiver, sizeof(receiver));
  } else {
    sockaddr_in6 receiver = {};
    receiver.sin6_family = AF_INET6;
    receiver.sin6_port = endpoint->port;
    memcpy(receiver.sin6_addr.s6_addr, endpoint->in6_addr, 16);
    res = bind(socket, (SOCKADDR*)&receiver, sizeof(receiver));
  }

#ifdef __COMPILE_AS_WINDOWS__
  if (res == SOCKET_ERROR) {
      QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

qbResult qb_socket_listen(qbSocket socket, int backlog) {
  int res = listen(socket, backlog);

#ifdef __COMPILE_AS_WINDOWS__
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

qbResult qb_socket_accept(qbSocket socket, qbSocket* peer, qbEndpoint endpoint) {
  char addr[sizeof(sockaddr_in6)] = { 0 };
  int addr_len;

  qbSocket res = accept(socket, (sockaddr*)addr, &addr_len);
#ifdef __COMPILE_AS_WINDOWS__
  if (res == INVALID_SOCKET) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif

  *peer = res;
  if (endpoint) {
    if (addr_len == sizeof(sockaddr_in)) {
      sockaddr_in* addr_in = (sockaddr_in*)&addr;
      endpoint->af = QB_IPV4;
      endpoint->port = addr_in->sin_port;
      endpoint->in_addr = addr_in->sin_addr.s_addr;
    } else if (addr_len == sizeof(sockaddr_in6)) {
      sockaddr_in6* addr_in = (sockaddr_in6*)&addr;
      endpoint->af = QB_IPV6;
      endpoint->port = addr_in->sin6_port;
      memcpy(endpoint->in6_addr, addr_in->sin6_addr.s6_addr, 16);
    }
  }

  return QB_OK;
}

qbResult qb_socket_connect(qbSocket socket, qbEndpoint endpoint) {
  int res = 0;
  if (endpoint->af == QB_IPV4) {
    sockaddr_in peer = {};
    peer.sin_family = AF_INET;
    peer.sin_port = endpoint->port;
    peer.sin_addr.s_addr = endpoint->in_addr;
    res = connect(socket, (SOCKADDR*)&peer, sizeof(peer));
  } else {
    sockaddr_in6 peer = {};
    peer.sin6_family = AF_INET6;
    peer.sin6_port = endpoint->port;
    memcpy(peer.sin6_addr.s6_addr, endpoint->in6_addr, 16);
    res = connect(socket, (SOCKADDR*)&peer, sizeof(peer));
  }

#ifdef __COMPILE_AS_WINDOWS__
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

int32_t qb_socket_send(qbSocket socket, const char* buf, size_t len, int flags) {
  int res = send(socket, buf, (int)len, flags);
#ifdef __COMPILE_AS_WINDOWS__
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#endif
  return res;
}

int32_t qb_socket_sendto(qbSocket socket, const char* buf, size_t len, int flags, qbEndpoint endpoint) {
  int res = 0;
  if (endpoint->af == QB_IPV4) {
    sockaddr_in peer = {};
    peer.sin_family = AF_INET;
    peer.sin_port = endpoint->port;
    peer.sin_addr.s_addr = endpoint->in_addr;
    res = sendto(socket, buf, (int)len, flags, (SOCKADDR*)&peer, sizeof(peer));
  } else {
    sockaddr_in6 peer = {};
    peer.sin6_family = AF_INET6;
    peer.sin6_port = endpoint->port;
    memcpy(peer.sin6_addr.s6_addr, endpoint->in6_addr, 16);
    res = sendto(socket, buf, (int)len, flags, (SOCKADDR*)&peer, sizeof(peer));
  }
#ifdef __COMPILE_AS_WINDOWS__  
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#endif
  return res;
}

int32_t qb_socket_recv(qbSocket socket, char* buf, size_t len, int flags) {
  int res = recv(socket, buf, (int)len, flags);
#ifdef __COMPILE_AS_WINDOWS__  
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#endif
  return res;
}

int32_t qb_socket_recvfrom(qbSocket socket, char* buf, size_t len, int flags, qbEndpoint endpoint) {
  char addr[sizeof(sockaddr_in6)] = { 0 };
  int addr_len;

  int res = recvfrom(socket, buf, (int)len, flags, (sockaddr*)addr, &addr_len);
#ifdef __COMPILE_AS_WINDOWS__  
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
  MSG_PEEK;
#else
  if (res < 0) {
    QB_RETURN_SOCKET_MSG_ERROR;
  }
#endif
  if (endpoint) {
    if (addr_len == sizeof(sockaddr_in)) {
      sockaddr_in* addr_in = (sockaddr_in*)&addr;
      endpoint->af = QB_IPV4;
      endpoint->port = addr_in->sin_port;
      endpoint->in_addr = addr_in->sin_addr.s_addr;
    } else if (addr_len == sizeof(sockaddr_in6)) {
      sockaddr_in6* addr_in = (sockaddr_in6*)&addr;
      endpoint->af = QB_IPV6;
      endpoint->port = addr_in->sin6_port;
      memcpy(endpoint->in6_addr, addr_in->sin6_addr.s6_addr, 16);
    }
  }

  return res;
}

qbResult qb_socket_setopt(qbSocket socket, int level, int name, const char* val, int len) {
  int res = setsockopt(socket, level, name, val, len);
#ifdef __COMPILE_AS_WINDOWS__  
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

qbResult qb_socket_getopt(qbSocket socket, int level, int name, char* val, int* len) {
  int res = getsockopt(socket, level, name, val, len);
#ifdef __COMPILE_AS_WINDOWS__  
  if (res == SOCKET_ERROR) {
    QB_RETURN_SOCKET_ERROR;
  }
#else
  if (res < 0) {
    QB_RETURN_SOCKET_ERROR;
  }
#endif
  return QB_OK;
}

qbResult qb_endpoint(qbEndpoint endpoint, const char* addrs, int port) {
  int res = inet_pton(AF_INET, addrs, &endpoint->in_addr);
  if (res == 1) {
    endpoint->af = QB_IPV4;
    endpoint->port = htons(port);
  } else if (res == -1) {
    errno = QB_SOCKET_EAFNOSUPPORT;
    return QB_ERROR_SOCKET;
  }

  res = inet_pton(AF_INET6, addrs, &endpoint->in6_addr);
  if (res == 1) {
    endpoint->af = QB_IPV6;
    endpoint->port = htons(port);
  } else if (res == -1) {
    errno = QB_SOCKET_EAFNOSUPPORT;
    return QB_ERROR_SOCKET;
  }
  return QB_OK;
}