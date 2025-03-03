#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

void
print_ethernet_header(struct ethernet_header *ethernet_header) {
  if (!DEBUG_OUTPUT) {
    return;
  }

  printf("Ethernet:\n");
  printf(" destination MAC: ");
  print_mac_address(ethernet_header->destination_mac);

  printf(" source MAC: ");
  print_mac_address(ethernet_header->source_mac);
}

int
parse_ethernet_frame(struct packet *packet) {
  struct ethernet_header *ethernet_header = (struct ethernet_header *) (packet->buffer + VIRTIO_HDR_SIZE);
  packet->mac_header = ethernet_header;
  packet->network_header = packet->buffer + VIRTIO_HDR_SIZE + ETHERNET_HEADER_SIZE;

  if (DEBUG_OUTPUT) {
    printf("\n==================================\n");
  }

  print_ethernet_header(ethernet_header);

  if (ntohs(ethernet_header->type) == ETHERNET_TYPE_ARP) {
    parse_arp_header(packet);
  } else if (ntohs(ethernet_header->type) == ETHERNET_TYPE_IPV4) {
    parse_ipv4_header(packet);
  } else if (ntohs(ethernet_header->type) == ETHERNET_TYPE_IPV6) {
    if (DEBUG_OUTPUT) {
      printf("IPv6: not supported yet\n");
    }
  } else {
    panic("Unexpected packet type");
  }

  if (DEBUG_OUTPUT) {
    printf("==================================\n");
  }

  return 0;
}

void
ethernet_header_push(struct packet *packet, uint8 mac_destination[MAC_ADDRESS_LENGTH], uint16 transport_proto) {
  struct ethernet_header *ethernet_header = packet_push(packet, sizeof(struct ethernet_header));
  packet->mac_header = ethernet_header;

  memmove(ethernet_header->source_mac, net_dev.mac, MAC_ADDRESS_LENGTH);
  memmove(ethernet_header->destination_mac, mac_destination, MAC_ADDRESS_LENGTH);
  ethernet_header->type = htons(transport_proto);

  packet_push(packet, VIRTIO_HDR_SIZE);
}

void
send_ethernet(struct packet *packet, uint8 mac_destination[MAC_ADDRESS_LENGTH], uint16 proto) {
  struct ethernet_header *ethernet_header = packet->mac_header = packet_push(packet, sizeof(struct ethernet_header));

  memmove(ethernet_header->source_mac, net_dev.mac, MAC_ADDRESS_LENGTH);
  memmove(ethernet_header->destination_mac, mac_destination, MAC_ADDRESS_LENGTH);
  ethernet_header->type = htons(proto);

  packet_push(packet, VIRTIO_HDR_SIZE);

  virtio_write_packet(packet);
}
