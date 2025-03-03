#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

struct packet *
packet_alloc_tx() {
  struct packet *packet = kalloc();

  if (!packet) {
    panic("memory allocation error");
  }

  memset(packet, 0, PGSIZE);

  packet->buffer = (void *) ((uint8 *) packet + PGSIZE - 1);

  return packet;
}

struct packet *
packet_alloc_rx() {
  struct packet *packet = kalloc();

  if (!packet) {
    panic("memory allocation error");
  }

  memset(packet, 0, PGSIZE);

  packet->buffer = (void *) ((uint8 *) packet + sizeof(struct packet));

  return packet;
}

void *
packet_push(struct packet *packet, int len) {
  packet->buffer = (void *) ((uint8 *) packet->buffer - len);
  packet->len += len;

  return packet->buffer;
}
