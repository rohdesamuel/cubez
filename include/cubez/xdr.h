#ifndef XDR__H
#define XDR__H

#include "common.h"

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

namespace network {

// NOTE: All functions return numbfer of bytes written to buffer.
namespace pack {

// Packs a null-terminated string into buffer.
DLLEXPORT int String(uint8_t* buffer, size_t buffer_size, const char* c);

DLLEXPORT int Float64(uint8_t* buffer, double d);
DLLEXPORT int Float32(uint8_t* buffer, float f);

DLLEXPORT int Int64(uint8_t* buffer, int64_t i);
DLLEXPORT int Int32(uint8_t* buffer, int32_t i);
DLLEXPORT int Int16(uint8_t* buffer, int16_t i);
DLLEXPORT int Int8(uint8_t* buffer, int8_t i);

DLLEXPORT int Uint64(uint8_t* buffer, uint64_t u);
DLLEXPORT int Uint32(uint8_t* buffer, uint32_t u);
DLLEXPORT int Uint16(uint8_t* buffer, uint16_t u);
DLLEXPORT int Uint8(uint8_t* buffer, uint8_t u);

DLLEXPORT int Bool(uint8_t* buffer, bool b);

DLLEXPORT int Array(uint8_t* buffer, double* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, float* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int64_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int32_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int16_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int8_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint64_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint32_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint16_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint8_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, bool* elements, size_t length);

template<class El_>
DLLEXPORT int VarArray(uint8_t* buffer, El_* elements, size_t length) {
  buffer += Uint64(buffer, length);
  return Array(buffer, elements, length);
}

DLLEXPORT int Opaque(uint8_t* buffer, void* data, size_t size);
DLLEXPORT int Bytes(uint8_t* buffer, void* data, size_t size);

}  // namespace pack

namespace unpack {

// Packs a null-terminated string into buffer.
DLLEXPORT int String(uint8_t* buffer, char* c, size_t c_length);

DLLEXPORT int Float64(uint8_t* buffer, double* d);
DLLEXPORT int Float32(uint8_t* buffer, float* f);

DLLEXPORT int Int64(uint8_t* buffer, int64_t* i);
DLLEXPORT int Int32(uint8_t* buffer, int32_t* i);
DLLEXPORT int Int16(uint8_t* buffer, int16_t* i);
DLLEXPORT int Int8(uint8_t* buffer, int8_t* i);

DLLEXPORT int Uint64(uint8_t* buffer, uint64_t* u);
DLLEXPORT int Uint32(uint8_t* buffer, uint32_t* u);
DLLEXPORT int Uint16(uint8_t* buffer, uint16_t* u);
DLLEXPORT int Uint8(uint8_t* buffer, uint8_t* u);

DLLEXPORT int Bool(uint8_t* buffer, bool* b);

DLLEXPORT int Array(uint8_t* buffer, double* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, float* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int64_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int32_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int16_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, int8_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint64_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint32_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint16_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, uint8_t* elements, size_t length);
DLLEXPORT int Array(uint8_t* buffer, bool* elements, size_t length);

template<class El_>
int DLLEXPORT VarArray(uint8_t* buffer, El_* elements) {
  uint64_t length;
  buffer += Uint64(buffer, &length);
  return Array(buffer, elements, length);
}

DLLEXPORT int Opaque(uint8_t* buffer, void* data);
DLLEXPORT int Bytes(uint8_t* buffer, void* data, size_t size);

}  // namespace unpack

}  // namespace network


#endif  // XDR__H
