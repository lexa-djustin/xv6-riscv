#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

struct udp_header {
    uint16 source;  // исходный порт
    uint16 dest;    // порт назначения
    uint16 len;     // длина UDP-заголовка и данных
    uint16 check;   // контрольная сумма
} __attribute__((packed));

struct pseudo_header {
    uint32 source_ip;
    uint32 dest_ip;
    uint8 zero;
    uint8 protocol;
    uint16 length;
} __attribute__((packed));

void
print_udp_header(struct udp_header *udp_header) {
  if (!DEBUG_OUTPUT) {
    return;
  }

  printf("UDP:\n");

  printf(" source port: %d\n", ntohs(udp_header->source));
  printf(" destination port: %d\n", ntohs(udp_header->dest));
  printf(" length: %d\n", ntohs(udp_header->len));
  printf(" payload: %s\n", (uint8 *) udp_header + sizeof(struct udp_header));
}


uint16
calculate_udp_checksum(struct udp_header *udp_header, uint32 source_ip, uint32 dest_ip) {
  struct pseudo_header pseudo_header;
  pseudo_header.source_ip = htonl(source_ip);
  pseudo_header.dest_ip = htonl(dest_ip);
  pseudo_header.zero = 0;
  pseudo_header.protocol = IP_PROTO_UDP;
  pseudo_header.length = udp_header->len;

  uint32 checksum = 0;
  checksum += calculate_checksum((uint8 *) &pseudo_header, sizeof(struct pseudo_header));
  checksum += calculate_checksum((uint8 *) udp_header, ntohs(udp_header->len));

  return htons(fin_checksum(checksum));
}

int
parse_udp_header(struct packet *packet) {
  struct udp_header *udp_header = (struct udp_header *) packet->transport_header;
  struct ipv4_header *ipv4_header = (struct ipv4_header *) packet->network_header;
  uint16 checksum_current = read_u16_be((uint8 *) &udp_header->check);

  udp_header->check = 0;

  if (calculate_udp_checksum(udp_header, ntohl(ipv4_header->saddr), ntohl(ipv4_header->daddr)) !=
      htons(checksum_current)) {
    printf("#####udp checksum is wrong#####\n");

    return -1;
  }

  print_udp_header(udp_header);

  struct socket *socket = get_socket(ntohs(udp_header->dest));

  if (socket == NULL) {
    return -1;
  }

  uint16 payload_len = ntohs(udp_header->len) - sizeof(struct udp_header);
  socket_add_to_rx_queue(socket, (uint8 *) udp_header + sizeof(struct udp_header), payload_len);

  wakeup(socket);

  return 0;
}

void
send_udp(struct packet *packet, int len, int local_port, int remote_port, unsigned int address) {
  struct udp_header *udp_header = packet->transport_header = packet_push(packet, sizeof(struct udp_header));
  uint16 udp_len = sizeof(struct udp_header) + len;

  udp_header->len = htons(udp_len);
  udp_header->source = htons(local_port);
  udp_header->dest = htons(remote_port);
  udp_header->check = calculate_udp_checksum(udp_header, net_dev.ip, address);

  send_ipv4(packet, IP_PROTO_UDP, udp_len, address);
}
