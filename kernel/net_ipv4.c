#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

static int is_fragmented(struct ipv4_header *ip) {
  return (ntohs(ip->frag_off) & (IP_MF | IP_OFFSET_MASK)) != 0;
}

void
print_ip_v4_header(struct ipv4_header *ipv4_header) {
  if (!DEBUG_OUTPUT) {
    return;
  }

  printf("IPv4:\n");
  printf(" total length: %d\n", ntohs(ipv4_header->tot_len));

  printf(" sender IP: ");
  print_ip(ntohl(ipv4_header->saddr));

  printf(" target IP: ");
  print_ip(ntohl(ipv4_header->daddr));

  printf(" ttl: %d\n", ipv4_header->ttl);
}

int
parse_ipv4_header(struct packet *packet) {
  struct ipv4_header *ipv4_header = (struct ipv4_header *) packet->network_header;
  int length = ipv4_header->ihl * 4;
  packet->transport_header = packet->network_header + length;
  uint16 checksum_current = read_u16_be((uint8 *) &ipv4_header->check);

  ipv4_header->check = 0;

  uint16 checksum_calculated = calculate_checksum_total((uint8 *) ipv4_header, ipv4_header->ihl * 4);

  if (checksum_current != checksum_calculated) {
    printf("#####ipv4 checksum is wrong#####\n");

    return -1;
  }

  if (ipv4_header->ttl == 0) {
    printf("Time Exceeded\n");

    return -1;
  }

  if (ntohl(ipv4_header->daddr) != net_dev.ip) {
    printf("Packet is not for us");

    return -1;
  }

  if (is_fragmented(ipv4_header)) {
    printf("Fragmentation is not supported\n");

    return -1;
  }

  arp_add_mac(ntohl(ipv4_header->saddr), ((struct ethernet_header *) packet->mac_header)->source_mac);
  print_ip_v4_header(ipv4_header);

  if (ipv4_header->protocol == IP_PROTO_UDP) {
    parse_udp_header(packet);
  } else if (ipv4_header->protocol == IP_PROTO_TCP) {
    parse_tcp_header(packet);
  } else if (ipv4_header->protocol == IP_PROTO_ICMP) {
    parse_icmp_header(packet, ntohs(ipv4_header->tot_len) - length);
  }

  return 0;
}

void
send_ipv4(struct packet *packet, uint8 proto, uint16 payload_len, uint32 ip_dest) {
  struct ipv4_header *ipv4_header = packet->network_header = packet_push(packet, sizeof(struct ipv4_header));

  ipv4_header->version = 4;
  ipv4_header->ihl = sizeof(struct ipv4_header) / 4;
  ipv4_header->tos = 0;
  ipv4_header->tot_len = htons(sizeof(struct ipv4_header) + payload_len);
  ipv4_header->id = 0;
  ipv4_header->frag_off = htons(IP_DF);
  ipv4_header->ttl = 64;
  ipv4_header->protocol = proto;
  ipv4_header->saddr = htonl(net_dev.ip);
  ipv4_header->daddr = htonl(ip_dest);
  ipv4_header->check = htons(calculate_checksum_total((uint8 *) ipv4_header, sizeof(struct ipv4_header)));

  uint8 dest_mac[MAC_ADDRESS_LENGTH];

  if (arp_resolve(ip_dest, dest_mac) == -1) {
    panic("ARP entry is NULL");
  }

  send_ethernet(packet, dest_mac, ETHERNET_TYPE_IPV4);
}
