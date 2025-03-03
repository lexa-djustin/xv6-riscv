#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

uint32
calculate_checksum(const uint8 *data, int len) {
  uint32 sum = 0;

  for (int i = 0; i < len - 1; i += 2) {
    sum += ((uint16) data[i] << 8) | data[i + 1];
  }

  if (len & 1) {
    sum += ((uint16) data[len - 1] << 8);
  }

  return sum;
}

uint16
fin_checksum(uint32 sum) {
  while (sum > 0xFFFF) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  return (uint16) ~sum;
}

uint16
calculate_checksum_total(const uint8 *data, int len) {
  uint32 sum = calculate_checksum(data, len);

  return fin_checksum(sum);
}