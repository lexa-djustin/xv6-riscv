#include "types.h"

uint16
htons(uint16 hostshort);

uint16
ntohs(uint16 netshort);

uint32
htonl(uint32 hostlong);

uint32
ntohl(uint32 netlong);

uint16
read_u16_be(const uint8 *buf);
