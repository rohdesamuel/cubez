#ifndef CUBEZ_NETWORK__H
#define CUBEZ_NETWORK__H

#include <cubez/common.h>
#include <cubez/socket.h>
#include <cubez/cubez.h>

typedef struct qbPacket_ {
  uint16_t size;
  char data;
} *qbPacket, qbPacket_;

QB_API int32_t qb_packet_send(qbSocket socket, qbPacket packet);
QB_API int32_t qb_packet_sendto(qbSocket socket, qbPacket packet, qbEndpoint endpoint);

QB_API int32_t qb_packet_recv(qbSocket socket, qbBuffer_* buf,
                              void(*recv)(qbVar, qbPacket), qbVar state);
QB_API int32_t qb_packet_recvfrom(qbSocket socket, qbBuffer_* buf, qbEndpoint endpoint,
                                  void(*recv)(qbVar, qbPacket), qbVar state);

#endif  // CUBEZ_NETWORK__H