#include "xdr.h"
#ifdef __COMPILE_AS_LINUX__
#include <arpa/inet.h>
#else
#include <Winsock2.h>
#endif
#include <memory.h>

namespace network {
namespace pack {

int String(uint8_t* buffer, size_t buffer_size, const char* c) {
  STRCPY((char*)buffer, buffer_size, c);
  return strlen((char*)buffer);
}

int Float64(uint8_t* buffer, double d) {
  uint8_t buff[8];
  *(double*)(buff) = d;
  *(double*)(buffer) = htonll(*(uint64_t*)buff);
  return 8;
}

int Float32(uint8_t* buffer, float f) {
  uint8_t buff[4];
  *(float*)(buff) = f;
  *(float*)(buffer) = htonl(*(uint32_t*)buff);
  return 4;
}

int Int64(uint8_t* buffer, int64_t i) {
  *(int64_t*)(buffer) = htonll(i);
  return 8;
}

int Int32(uint8_t* buffer, int32_t i) {
  *(int32_t*)(buffer) = htonl(i);
  return 4;
}

int Int16(uint8_t* buffer, int16_t i) {
  *(int16_t*)(buffer) = htons(i);
  return 2;
}

int Int8(uint8_t* buffer, int8_t i) {
  *buffer = i;
  return 1;
}

int Uint64(uint8_t* buffer, uint64_t i) {
  *(uint64_t*)(buffer) = htonll(i);
  return 8;
}

int Uint32(uint8_t* buffer, uint32_t i) {
  *(uint32_t*)(buffer) = htonl(i);
  return 4;
}

int Uint16(uint8_t* buffer, uint16_t i) {
  *(uint16_t*)(buffer) = htons(i);
  return 2;
}

int Uint8(uint8_t* buffer, uint8_t i) {
  *buffer = i;
  return 1;
}

int Bool(uint8_t* buffer, bool b) {
  return Uint8(buffer, b);
}

int Array(uint8_t* buffer, double* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Float64(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, float* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Float32(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int64_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int64(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int32_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int32(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int16_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int16(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int8_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int8(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint64_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint64(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint32_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint32(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint16_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint16(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint8_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint8(buffer, elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, bool* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Bool(buffer, elements[i]);
  }
  return length;
}

int Opaque(uint8_t* buffer, void* data, size_t size) {
  buffer += Uint64(buffer, size);
  memmove(buffer, data, size);
  return size;
}

int Bytes(uint8_t* buffer, void* data, size_t size) {
  memmove(buffer, data, size);
  return size;
}

}  // namespace pack

namespace unpack {

int String(uint8_t* buffer, char* c, size_t c_length) {
  STRCPY(c, c_length, (char*)buffer);
  return strlen(c);
}

int Float64(uint8_t* buffer, double* d) {
  *d = ntohll(*(uint64_t*)buffer);
  return 8;
}

int Float32(uint8_t* buffer, float* f) {
  *f = ntohl(*(uint32_t*)buffer);
  return 4;
}

int Int64(uint8_t* buffer, int64_t* i) {
  *i = htonll(*(int64_t*)buffer);
  return 8;
}

int Int32(uint8_t* buffer, int32_t* i) {
  *i = htonl(*(int32_t*)buffer);
  return 4;
}

int Int16(uint8_t* buffer, int16_t* i) {
  *i = htons(*(int16_t*)buffer);
  return 2;
}

int Int8(uint8_t* buffer, int8_t* i) {
  *i = *buffer;
  return 1;
}

int Uint64(uint8_t* buffer, uint64_t* i) {
  *i = htonll(*(uint64_t*)buffer);
  return 8;
}

int Uint32(uint8_t* buffer, uint32_t* i) {
  *i = htonl(*(uint32_t*)buffer);
  return 4;
}

int Uint16(uint8_t* buffer, uint16_t* i) {
  *i = htons(*(uint16_t*)buffer);
  return 2;
}

int Uint8(uint8_t* buffer, uint8_t* i) {
  *i = *buffer;
  return 1;
}

int Bool(uint8_t* buffer, bool* b) {
  *b = *buffer;
  return 1;
}

int Array(uint8_t* buffer, double* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Float64(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, float* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Float32(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int64_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int64(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int32_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int32(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int16_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int16(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, int8_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Int8(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint64_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint64(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint32_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint32(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint16_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint16(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, uint8_t* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Uint8(buffer, &elements[i]);
  }
  return length;
}

int Array(uint8_t* buffer, bool* elements, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    buffer += Bool(buffer, &elements[i]);
  }
  return length;
}

int Opaque(uint8_t* buffer, void* data) {
  size_t size;
  buffer += Uint64(buffer, &size);
  memmove(data, buffer, size);
  return size;
}

int Bytes(uint8_t* buffer, void* data, size_t size) {
  memmove(data, buffer, size);
  return size;
}


}  // namespace unpack


}  // namespace network
