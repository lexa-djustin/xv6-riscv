#define MAC_ADDRESS_LENGTH 6
#define PACKET_RX_QUEUE_SIZE 256
#define MAX_PORTS 65535
#define DEBUG_OUTPUT 0

#define ETHERNET_HEADER_SIZE sizeof(struct ethernet_header)
#define VIRTIO_HDR_SIZE sizeof(struct virtio_net_hdr)
#define ARP_HEADER_SIZE sizeof(struct arp_header)
#define MAX_PAYLOAD_SIZE 1500
#define ARP_TABLE_SIZE 16
#define NET_HEADROOM 64

#define IP_MF 0x2000
#define IP_OFFSET_MASK 0x1FFF
#define IP_DF 0x4000

static const uint16 ETHERNET_TYPE_IPV4 = 0x0800;
static const uint16 ETHERNET_TYPE_IPV6 = 0x86DD;
static const uint16 ETHERNET_TYPE_ARP = 0x0806;

static const uint8 IP_PROTO_TCP = 6;
static const uint8 IP_PROTO_UDP = 17;
static const uint8 IP_PROTO_ICMP = 1;

struct dev;

struct net_device {
    struct dev *dev;
    struct spinlock lock;
    uint8 mac[MAC_ADDRESS_LENGTH];
    uint32 ip;
};

struct virtio_net_hdr {
    uint8 flags;
    uint8 gso_type;
    uint16 hdr_len;
    uint16 gso_size;
    uint16 csum_start;
    uint16 csum_offset;
};

extern struct net_device net_dev;

struct packet {
    struct socket *socket;
    uint32 len;
    void *mac_header;
    void *network_header;
    void *transport_header;
    void *buffer;
};

struct ipv4_header {
    uint8 ihl: 4;      // длина заголовка
    uint8 version: 4;  // версия протокола
    uint8 tos;         // тип обслуживания
    uint16 tot_len;    // общая длина пакета
    uint16 id;         // идентификатор пакета
    uint16 frag_off;   // флаги и смещение фрагмента
    uint8 ttl;         // время жизни
    uint8 protocol;    // протокол следующего уровня (например, TCP, UDP)
    uint16 check;      // контрольная сумма заголовка
    uint32 saddr;      // IP-адрес источника
    uint32 daddr;      // IP-адрес назначения
} __attribute__((packed));

struct ethernet_header {
    uint8 destination_mac[MAC_ADDRESS_LENGTH];
    uint8 source_mac[MAC_ADDRESS_LENGTH];
    uint16 type;
};


struct rx_packet_queue {
    struct packet *packets[PACKET_RX_QUEUE_SIZE];
    struct spinlock lock;
    int nread;
    int nwrite;
};

struct port_table {
    struct socket *sockets[MAX_PORTS];
    struct spinlock lock;
};

struct arp_entry {
    uint32 ip;
    uint8 mac[MAC_ADDRESS_LENGTH];
};

extern struct rx_packet_queue netq;

void init_rx_packet_queue(struct rx_packet_queue *queue);

struct packet *packet_alloc_rx();

int enqueue_packet(void *raw_packet);

void *dequeue_packet();

void
send_ipv4(struct packet *packet, uint8 proto, uint16 payload_len, uint32 ip_dest);

void
ethernet_header_push(struct packet *packet, uint8 mac_destination[MAC_ADDRESS_LENGTH], uint16 transport_proto);

void
send_ethernet(struct packet *packet, uint8 mac_destination[MAC_ADDRESS_LENGTH], uint16 proto);

void
virtio_write_packet(struct packet *packet);

uint32
calculate_checksum(const uint8 *data, int len);

uint16
fin_checksum(uint32 sum);

uint16
calculate_checksum_total(const uint8 *data, int len);

struct socket *
get_socket(uint16 port);

int
arp_get_mac(uint32 ip, uint8 mac[MAC_ADDRESS_LENGTH]);

int
arp_add_mac(uint32 ip, uint8 mac[MAC_ADDRESS_LENGTH]);

int
arp_resolve(uint32 ip, uint8 mac[MAC_ADDRESS_LENGTH]);

int
parse_udp_header(struct packet *packet);

int
arp_has_mac(uint32 ip);

void
send_udp(struct packet *packet, int len, int local_port, int remote_port, unsigned int address);

void
send_arp(uint32 ip);

int
parse_arp_header(struct packet *packet);

int
parse_tcp_header(struct packet *packet);

int
parse_icmp_header(struct packet *packet, int icmp_packet_size);

int
parse_ipv4_header(struct packet *packet);

int
parse_ethernet_frame(struct packet *packet);

void
print_mac_address(const uint8 *mac);

void
print_ip(uint32 ip);
