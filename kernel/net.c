#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "net.h"
#include "socket.h"

#define EPHEMERAL_PORT_START 32770
#define EPHEMERAL_PORT_END 60999

struct net_device net_dev;
struct rx_packet_queue netq;
struct port_table port_table;

void init_rx_packet_queue(struct rx_packet_queue *queue) {
  initlock(&queue->lock, "netq");
}

int get_free_port() {
  for (int port = 1; port < MAX_PORTS; port++) {
    if (port_table.sockets[port] == NULL) {
      return port;
    }
  }

  return -1;
}

static inline int try_to_allocate_port(struct socket *socket, uint32 port)
{
  if (port_table.sockets[port] == NULL) {
    port_table.sockets[port] = socket;
    socket->local_port = port;

    return 0;
  }

  return -1;
}

int allocate_port(struct socket *socket) {
  int attempts = 10;

  while (attempts-- != 0) {
    uint32 random_port = rand_range(EPHEMERAL_PORT_START, EPHEMERAL_PORT_END);

    if (try_to_allocate_port(socket, random_port) == -1) {
      continue;
    }

    return 0;
  }

  for (int port = EPHEMERAL_PORT_START; port <= EPHEMERAL_PORT_END; port++) {
    if (try_to_allocate_port(socket, port) == 0) {
      return 0;
    }
  }

  return -1;
}

int add_socket(struct socket *socket) {
  if (port_table.sockets[socket->local_port] == NULL) {
    port_table.sockets[socket->local_port] = socket;

    return 0;
  }

  return -1;
}

struct socket *
get_socket(uint16 port) {
  return port_table.sockets[port];
}

void release_socket(struct socket *socket) {
  port_table.sockets[socket->local_port] = NULL;
}

void print_mac_address(const uint8 *mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) {
      printf("0");
    }

    printf("%x", mac[i]);

    if (i < 5) {
      printf(":");
    }
  }

  printf("\n");
}

void
print_ip(uint32 ip) {
  uint8 bytes[4];

  bytes[0] = (ip >> 24) & 0xFF;
  bytes[1] = (ip >> 16) & 0xFF;
  bytes[2] = (ip >> 8) & 0xFF;
  bytes[3] = ip & 0xFF;

  printf("%d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void process_net_queue() {
  void *dequeued_packet;

  while ((dequeued_packet = (void *) dequeue_packet())) {
    parse_ethernet_frame((struct packet *) dequeued_packet);
    kfree(dequeued_packet);
  }
}

int
enqueue_packet(void *raw_packet) {
  acquire(&netq.lock);

  int next = (netq.nwrite + 1) % PACKET_RX_QUEUE_SIZE;

  if (next == netq.nread) {
    release(&netq.lock);

    return -1;
  }

  netq.packets[netq.nwrite] = raw_packet;
  __sync_synchronize();
  netq.nwrite = next;

  release(&netq.lock);

  return 0;
}

void *dequeue_packet() {
  acquire(&netq.lock);

  if (netq.nread == netq.nwrite) {
    release(&netq.lock);

    return (void *) 0;
  }

  void *raw_packet = netq.packets[netq.nread];
  netq.nread = (netq.nread + 1) % PACKET_RX_QUEUE_SIZE;

  release(&netq.lock);

  return raw_packet;
}
