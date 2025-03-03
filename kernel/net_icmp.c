#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "net_byteorder.h"

#define ICMP_ECHO_REPLY        0
#define ICMP_DEST_UNREACH      3
#define ICMP_SOURCE_QUENCH     4
#define ICMP_REDIRECT          5
#define ICMP_ECHO_REQUEST      8
#define ICMP_TIME_EXCEEDED    11
#define ICMP_PARAMETERPROB    12
#define ICMP_TIMESTAMP_REQUEST 13
#define ICMP_TIMESTAMP_REPLY  14
#define ICMP_ADDRESS_MASK_REQUEST 17
#define ICMP_ADDRESS_MASK_REPLY  18

struct icmp_header {
    uint8 type;        // Тип ICMP (например, 8 для эхо-запроса, 0 для эхо-ответа)
    uint8 code;        // Код сообщения (обычно 0 для эхо-запроса/ответа)
    uint16 checksum;   // Контрольная сумма для проверки целостности пакета
    union {
        struct {         // Используется для эхо-запроса/ответа
            uint16 id;       // Идентификатор, позволяющий сопоставить запрос и ответ
            uint16 sequence; // Номер последовательности
        } echo;
        struct {         // Пример для других типов сообщений ICMP
            uint32 unused;   // Зарезервировано или не используется
        } other;
    } un;
} __attribute__((packed));

void
print_icmp_header(struct icmp_header *icmp_header) {
  if (!DEBUG_OUTPUT) {
    return;
  }

  printf("ICMP:\n");
  printf(" type: %d\n", icmp_header->type);
  printf(" code: %d\n", icmp_header->code);
}

void
send_icmp_echo_reply(struct icmp_header *icmp_header_request, int icmp_packet_size, uint32 ip_dest) {
  struct packet *packet = packet_alloc_tx();
  struct icmp_header *icmp_header_response = packet->transport_header = packet_push(packet, icmp_packet_size);

  memmove(icmp_header_response, icmp_header_request, icmp_packet_size);

  icmp_header_response->type = 0;
  icmp_header_response->code = 0;
  icmp_header_response->checksum = htons(calculate_checksum_total((uint8 *) icmp_header_response, icmp_packet_size));

  send_ipv4(packet, IP_PROTO_ICMP, icmp_packet_size, ip_dest);
}

int
parse_icmp_header(struct packet *packet, int icmp_packet_size) {
  struct icmp_header *icmp_header = (struct icmp_header *) packet->transport_header;
  struct ipv4_header *ipv4_header = (struct ipv4_header *) packet->network_header;
  uint16 checksum = read_u16_be((uint8 *) &icmp_header->checksum);

  icmp_header->checksum = 0;

  if (calculate_checksum_total((uint8 *) icmp_header, icmp_packet_size) != checksum) {
    printf("#####icmp checksum is wrong#####\n");
  }

  print_icmp_header(icmp_header);

  if (icmp_header->type == ICMP_ECHO_REQUEST) {
    send_icmp_echo_reply(icmp_header, icmp_packet_size, ntohl(ipv4_header->saddr));
  } else {
    printf("ICMP type (%d) is not supporting yet\n", icmp_header->type);
  }

  return 0;
}
