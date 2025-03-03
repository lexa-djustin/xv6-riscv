#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

struct arp_header {
    uint16 hw_type;
    uint16 proto_type;
    uint8 hw_size;
    uint8 proto_size;
    uint16 opcode;
    uint8 sender_mac[6];
    uint32 sender_ip;
    uint8 target_mac[6];
    uint32 target_ip;
} __attribute__((packed));

struct arp_table {
    struct arp_entry entries[ARP_TABLE_SIZE];
    struct spinlock lock;
};

struct arp_table arp_table;

int
arp_get_mac(uint32 ip, uint8 mac[MAC_ADDRESS_LENGTH]) {
  acquire(&arp_table.lock);

  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table.entries[i].ip == ip) {
      memmove(mac, arp_table.entries[i].mac, MAC_ADDRESS_LENGTH);
      release(&arp_table.lock);
      return 0;
    }
  }

  release(&arp_table.lock);
  return -1;
}

int
arp_add_mac(uint32 ip, uint8 mac[MAC_ADDRESS_LENGTH]) {
  acquire(&arp_table.lock);

  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table.entries[i].ip == 0) {
      arp_table.entries[i].ip = ip;
      memmove(arp_table.entries[i].mac, mac, MAC_ADDRESS_LENGTH);
      release(&arp_table.lock);
      return 0;
    }
  }

  release(&arp_table.lock);
  return -1;
}

int
arp_has_mac(uint32 ip) {
  acquire(&arp_table.lock);

  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table.entries[i].ip == ip) {
      release(&arp_table.lock);
      return 1;
    }
  }

  release(&arp_table.lock);
  return 0;
}

int
arp_resolve(uint32 ip, uint8 mac[MAC_ADDRESS_LENGTH]) {
  if (arp_get_mac(ip, mac) == -1) {
    send_arp(ip);
  }

  return arp_get_mac(ip, mac);
}


void
print_arp_header(struct arp_header *arp_header) {
  if (!DEBUG_OUTPUT) {
    return;
  }

  printf("ARP:\n");

  printf(" opcode: %d\n", ntohs(arp_header->opcode));
  printf(" sender MAC address: ");
  print_mac_address(arp_header->sender_mac);
  printf(" destination MAC address: ");
  print_mac_address(arp_header->target_mac);

  printf(" Sender IP: ");
  print_ip(ntohl(arp_header->sender_ip));

  printf(" Target IP: ");
  print_ip(ntohl(arp_header->target_ip));
}

int
parse_arp_header(struct packet *packet) {
  struct arp_header *arp = (struct arp_header *) packet->network_header;
  struct ethernet_header *ethernet_header = (struct ethernet_header *) packet->mac_header;
  uint16 opcode = ntohs(arp->opcode);

  print_arp_header(arp);

  if (opcode == 1) {
    struct packet *response_packet = packet_alloc_tx();
    struct arp_header *response_arp = packet_push(response_packet, sizeof(struct arp_header));
    response_packet->network_header = (void*) response_arp;

    response_arp->opcode = htons(2);
    response_arp->hw_type = htons(1);
    response_arp->hw_size = 6;
    response_arp->proto_size = 4;
    response_arp->proto_type = htons(0x0800);
    response_arp->sender_ip = htonl(net_dev.ip);
    response_arp->target_ip = arp->sender_ip;

    memmove(response_arp->target_mac, arp->sender_mac, MAC_ADDRESS_LENGTH);
    memmove(response_arp->sender_mac, net_dev.mac, MAC_ADDRESS_LENGTH);

    send_ethernet(response_packet, ethernet_header->source_mac, ETHERNET_TYPE_ARP);
  } else if (opcode == 2) {
    arp_add_mac(ntohl(arp->sender_ip), arp->sender_mac);
    wakeup((void *) (uint64) ntohl(arp->sender_ip));
  } else {
    panic("ARP opcode not supported");
  }

  return 0;
}

void
send_arp(uint32 ip) {
  struct packet *packet = packet_alloc_tx();
  struct arp_header *arp = packet->network_header = packet_push(packet, sizeof(struct arp_header));

  arp->opcode = htons(1);
  arp->hw_type = htons(1);
  arp->hw_size = 6;
  arp->proto_size = 4;
  arp->proto_type = htons(0x0800);
  arp->sender_ip = htonl(net_dev.ip);
  arp->target_ip = htonl(ip);
  memmove(arp->sender_mac, net_dev.mac, MAC_ADDRESS_LENGTH);

  uint8 broadcast_mac[MAC_ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  send_ethernet(packet, broadcast_mac, ETHERNET_TYPE_ARP);

  if (myproc()) {
    acquire(&net_dev.lock);
    // Check if address still is not received
    if (arp_has_mac(ip) == 0) {
      sleep((void *) (uint64) ip, &net_dev.lock);
    }

    release(&net_dev.lock);
  }
}
