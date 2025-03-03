#include "types.h"

uint16 htons(uint16 hostshort) {
  return ((hostshort & 0xFF) << 8) | ((hostshort & 0xFF00) >> 8);
}

uint16 ntohs(uint16 netshort) {
  return htons(netshort);
}

uint32 htonl(uint32 hostlong) {
  return
          (hostlong & 0xFF) << 24 |
          (hostlong & 0xFF00) << 8 |
          (hostlong & 0xFF0000) >> 8 |
          (hostlong & 0xFF000000) >> 24;
}

uint32 ntohl(uint32 netlong) {
  return htonl(netlong);
}

uint16 read_u16_be(const uint8 *buf) {
  return buf[0] << 8 | buf[1];
}
