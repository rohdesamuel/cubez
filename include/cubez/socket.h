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

#ifndef CUBEZ_SOCKET__H
#define CUBEZ_SOCKET__H

#include <cubez/common.h>

#ifdef __COMPILE_AS_WINDOWS__
#if defined(_WIN64)
typedef int64_t qbSocket;
#else
typedef int32_t qbSocket;
#endif
#else
typedef int qbSocket;
#endif

typedef enum qbNetworkProtocol {
  QB_UDP,  // Maps to SOCK_DGRAM
  QB_TCP,  // Maps to SOCK_STREAM
} qbNetworkProtocol;

typedef enum qbAddressFamily{
  QB_IPV4,  // Maps to AF_INET
  QB_IPV6,  // Maps to AF_INET6
} qbAddressFamily;

typedef struct qbSocketAttr_ {
  qbNetworkProtocol np;
  qbAddressFamily af;
} qbSocketAttr_, *qbSocketAttr;

typedef struct qbEndpoint_ {
  qbAddressFamily af;

  // Port in network order.
  uint16_t port;
  union {
    // Address in network order.
    uint32_t in_addr;
    uint8_t in6_addr[16];
  };
} qbEndpoint_, *qbEndpoint;

// https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send
// https://man7.org/linux/man-pages/man7/tcp.7.html
// https://man7.org/linux/man-pages/man7/ip.7.html
// https://man7.org/linux/man-pages/man7/udp.7.html
typedef enum qbSocketError {
  QB_SOCKET_OK = 0,
  QB_SOCKET_UNKNOWN = INT8_MIN,
  QB_SOCKET_EACCES,
  QB_SOCKET_EADDRINUSE,
  QB_SOCKET_EADDRNOTAVAIL,
  QB_SOCKET_EAFNOSUPPORT,
  QB_SOCKET_EALREADY,    
  QB_SOCKET_ECONNABORTED,
  QB_SOCKET_ECONNREFUSED,
  QB_SOCKET_ECONNRESET,
  QB_SOCKET_EDESTADDRREQ,
  QB_SOCKET_EFAULT,  
  QB_SOCKET_EHOSTDOWN,
  QB_SOCKET_EHOSTUNREACH,
  QB_SOCKET_EINPROGRESS,
  QB_SOCKET_EINTR,
  QB_SOCKET_EINVAL,
  QB_SOCKET_EISCONN,
  QB_SOCKET_EMFILE,
  QB_SOCKET_EMSGSIZE,
  QB_SOCKET_ENETUNREACH,
  QB_SOCKET_ENOBUFS,
  QB_SOCKET_ENOPROTOOPT,
  QB_SOCKET_ENOTCONN,
  QB_SOCKET_ENOTSOCK,
  QB_SOCKET_EOPNOTSUPP,
  QB_SOCKET_EPROTONOSUPPORT,
  QB_SOCKET_EPROTOTYPE,
  QB_SOCKET_ESHUTDOWN,
  QB_SOCKET_ESOCKTNOSUPPORT,
  QB_SOCKET_ETIMEDOUT,
  QB_SOCKET_EWOULDBLOCK,
} qbSocketError;

typedef enum qbSocketShutdown {
  QB_SOCKET_RECEIVE,
  QB_SOCKET_SEND,
  QB_SOCKET_BOTH,
} qbSocketShutdown;

#define QB_SOCKET_OOB 0x1
#define QB_SOCKET_PEEK 0x2
#define QB_SOCKET_DONTROUTE 0x4

QB_API qbResult qb_endpoint(qbEndpoint endpoint, const char* addrs, int port);

QB_API qbResult qb_socket_create(qbSocket* socket, qbSocketAttr attr);
QB_API qbResult qb_socket_close(qbSocket socket);
QB_API qbResult qb_socket_shutdown(qbSocket socket, qbSocketShutdown how);
QB_API qbResult qb_socket_bind(qbSocket socket, qbEndpoint endpoint);
QB_API qbResult qb_socket_listen(qbSocket socket, int backlog);
QB_API qbResult qb_socket_accept(qbSocket socket, qbSocket* peer, qbEndpoint endpoint);

QB_API qbResult qb_socket_connect(qbSocket socket, qbEndpoint endpoint);

QB_API int32_t qb_socket_send(qbSocket socket, const char* buf, size_t len, int flags);
QB_API int32_t qb_socket_sendto(qbSocket socket, const char* buf, size_t len, int flags, qbEndpoint endpoint);

QB_API int32_t qb_socket_recv(qbSocket socket, char* buf, size_t len, int flags);
QB_API int32_t qb_socket_recvfrom(qbSocket socket, char* buf, size_t len, int flags, qbEndpoint endpoint);

// See OS specific socket documentation for what options are available.
QB_API qbResult qb_socket_setopt(qbSocket socket, int level, int name, const char* val, int len);
QB_API qbResult qb_socket_getopt(qbSocket socket, int level, int name, char* val, int* len);

QB_API uint16_t qb_htons(uint16_t hostshort);
QB_API uint32_t qb_htonl(uint32_t hostlong);
QB_API uint64_t qb_htonll(uint64_t hostll);
QB_API uint32_t qb_htonf(float hostfloat);
QB_API uint64_t qb_htond(double hostdouble);

QB_API uint16_t qb_ntohs(uint16_t netshort);
QB_API uint32_t qb_ntohl(uint32_t netlong);
QB_API uint64_t qb_ntohll(uint64_t netll);
QB_API float qb_ntohf(uint32_t netfloat);
QB_API double qb_ntohd(uint64_t netdouble);

#endif  // CUBEZ_SOCKET__H