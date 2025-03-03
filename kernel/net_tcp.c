#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "net.h"
#include "socket.h"
#include "net_byteorder.h"

#define IPV4_PAYLOAD_LEN(pkt) ({ \
    struct ipv4_header *ip = (struct ipv4_header *)(pkt)->network_header; \
    uint16 ip_total_len = ntohs(ip->tot_len); \
    uint8 ip_header_len = ip->ihl * 4; \
    (ip_total_len > ip_header_len) \
        ? (ip_total_len - ip_header_len) \
        : 0; \
})
#define min(a, b) ((a) < (b) ? (a) : (b))
#define SOCKETS_LEN 256
#define MTU 1400
#define TIME_WAIT_TIMEOUT_TICKS 30

#define TCP_HEADER_LEN(tcp_header_ptr)  ((tcp_header_ptr)->doff * 4)

#define TCP_FLAG_FIN  (1 << 0)  // Завершение отправки (Finish)
#define TCP_FLAG_SYN  (1 << 1)  // Установление соединения (Synchronize)
#define TCP_FLAG_RST  (1 << 2)  // Сброс соединения (Reset)
#define TCP_FLAG_PSH  (1 << 3)  // Принудительная доставка (Push)
#define TCP_FLAG_ACK  (1 << 4)  // Подтверждение (Acknowledgment)
#define TCP_FLAG_URG  (1 << 5)  // Срочные данные (Urgent)
#define TCP_FLAG_ECE  (1 << 6)  // ECN Echo
#define TCP_FLAG_CWR  (1 << 7)  // Congestion Window Reduced (CWR)

// Connection establishment

// | Direction       | `seq` | `ack_seq` | Flags     |
// | --------------- | ----- | --------- | --------- |
// | Client → Server | 1000  | 0         | SYN       |
// | Server → Client | 3000  | 1001      | SYN + ACK |
// | Client → Server | 1001  | 3001      | ACK       |

// TCP_CLOSED
//     |
//     | connect() / send SYN
//     v
// TCP_SYN_SENT
//     |
//     | recv SYN+ACK → send ACK
//     v
// TCP_ESTABLISHED



// Terminating the connection

// Client -> Server

// | Direction       | `seq` | `ack_seq` | Flags     |
// | --------------- | ----- | --------- | --------- |
// | Client → Server | 1000  | 2000      | FIN + ACK |
// | Server → Client | 2000  | 1001      | ACK       |
// | Server → Client | 2000  | 1001      | FIN + ACK |
// | Client → Server | 1001  | 2001      | ACK       |

// TCP_ESTABLISHED
//     |
//     | close() / send FIN
//     v
// TCP_FIN_WAIT_1
//     |
//     | recv ACK to our FIN
//     v
// TCP_FIN_WAIT_2
//     |
//     | recv FIN from peer
//     v
// TCP_TIME_WAIT
//     |
//     | wait 2MSL timeout
//     v
// TCP_CLOSED

// Server -> Client

// | Direction       | `seq` | `ack_seq` | Flags     |
// | --------------- | ----- | --------- | --------- |
// | Server → Client | 2000  | 1000      | FIN + ACK |
// | Client → Server | 1000  | 2001      | ACK       |
// | Client → Server | 1000  | 2001      | FIN + ACK |
// | Server → Client | 2001  | 1001      | ACK       |

// TCP_ESTABLISHED
//     |
//     | recv FIN from peer
//     v
// TCP_CLOSE_WAIT
//     |
//     | close() / send FIN
//     v
// TCP_LAST_ACK
//     |
//     | recv ACK to our FIN
//     v
// TCP_CLOSED


// | Поле                    | Назначение                                                |
// | ----------------------- | --------------------------------------------------------- |
// | `snd_nxt`               | Следующий SEQ, который будет отправлен                    |
// | `snd_una`               | Самый ранний SEQ, который ещё не подтверждён              |
// | `rcv_nxt`               | Следующий SEQ, который мы ожидаем от удалённой стороны    |
// | `snd_wnd`               | Окно удалённой стороны                                    |
// | `rcv_wnd`               | Наше окно — сколько байт мы готовы принять                |
// | `state`                 | Состояние соединения по TCP FSM                           |
// | `send_buf` / `recv_buf` | Буферы для данных, отправляемых/принимаемых пользователем |

struct tcp_header {
    uint16 source;
    uint16 dest;
    uint32 seq;
    uint32 ack_seq;

    uint16 res1: 4;
    uint16 doff: 4;
    uint16 fin: 1;
    uint16 syn: 1;
    uint16 rst: 1;
    uint16 psh: 1;
    uint16 ack: 1;
    uint16 urg: 1;
    uint16 ece: 1;
    uint16 cwr: 1;

    uint16 window;
    uint16 check;
    uint16 urg_ptr;
} __attribute__((packed));

struct pseudo_header {
    uint32 source_ip;
    uint32 dest_ip;
    uint8 zero;
    uint8 protocol;
    uint16 length;
} __attribute__((packed));

static int
check_tcp_packet_checksum(struct packet *packet);

static uint16
calculate_tcp_checksum(struct tcp_header *tcp_header, uint32 source_ip, uint32 dest_ip, void *payload,
                       uint32 payload_len);

static void
handle_tcp_syn_send(struct packet *packet);

static void
handle_tcp_established(struct packet *packet);

static void
handle_tcp_fin_wait_1(struct packet *packet);

static void
handle_tcp_fin_wait_2(struct packet *packet);

static void
handle_tcp_last_ack(struct packet *packet);

static void
handle_tcp_time_wait(struct packet *packet);

struct socket *sockets[SOCKETS_LEN] = {NULL};

static void
start_rto_timer(struct socket *socket);

static void
increase_rto_timer(struct socket *socket);

static void
stop_rto_timer(struct socket *socket);

static void
start_zwp_timer(struct socket *socket);

static void
increase_zwp_timer(struct socket *socket);

static void
stop_zwp_timer(struct socket *socket);

static void
start_time_wait_expires_timer(struct socket *socket);

static uint32
send_tcp_segment(struct socket *socket);

void
print_tcp_header(struct tcp_header *tcp_header) {
  if (!DEBUG_OUTPUT) {
    return;
  }

  printf("TCP:\n");

  printf(" source port: %d\n", tcp_header->source);
  printf(" destination port: %d\n", tcp_header->dest);
}

static const char *
tcp_state_to_string(enum socket_tcp_state state) {
  switch (state) {
    case TCP_CLOSED:
      return "TCP_CLOSED";
    case TCP_LISTEN:
      return "TCP_LISTEN";
    case TCP_SYN_SENT:
      return "TCP_SYN_SENT";
    case TCP_SYN_RECEIVED:
      return "TCP_SYN_RECEIVED";
    case TCP_ESTABLISHED:
      return "TCP_ESTABLISHED";
    case TCP_FIN_WAIT_1:
      return "TCP_FIN_WAIT_1";
    case TCP_FIN_WAIT_2:
      return "TCP_FIN_WAIT_2";
    case TCP_CLOSE_WAIT:
      return "TCP_CLOSE_WAIT";
    case TCP_CLOSING:
      return "TCP_CLOSING";
    case TCP_LAST_ACK:
      return "TCP_LAST_ACK";
    case TCP_TIME_WAIT:
      return "TCP_TIME_WAIT";
    default:
      return "UNKNOWN_TCP_STATE";
  }
}

void
change_state(struct socket *socket, enum socket_tcp_state state) {
  printf("state: %s -> %s\n", tcp_state_to_string(socket->state), tcp_state_to_string(state));

  socket->state = state;
}

int
connect_tcp(struct socket *socket) {
  uint32 isn;

  if (socket->state != TCP_CLOSED) {
    panic("connect_tcp: wrong socket state");
  }

  for (int i = 0; i < SOCKETS_LEN; i++) {
    if (sockets[i] == NULL) {
      sockets[i] = socket;

      printf("connect_tcp: local port: %d\n", socket->local_port);

      break;
    }
  }

  isn = rand();
  socket->rcv_wnd = 512;
  socket->snd_una = isn;
  socket->snd_nxt = isn;

  change_state(socket, TCP_SYN_SENT);
  send_tcp(socket, TCP_FLAG_SYN, NULL, 0);

  return 0;
}

int
close_tcp(struct socket *socket) {
  if (socket->state != TCP_ESTABLISHED) {
    panic("close_tcp: wrong socket state");
  }

  printf("snd_una: %d; snd_nxt: %d\n", socket->snd_una, socket->snd_nxt);
  process_net_queue();


//  while (socket->snd_una != socket->snd_nxt) {
//
//  }

  send_tcp(socket, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
  change_state(socket, TCP_FIN_WAIT_1);

  return 0;
}

int
parse_tcp_header(struct packet *packet) {
  struct tcp_header *tcp_header = (struct tcp_header *) packet->transport_header;
  struct socket *socket = get_socket(ntohs(tcp_header->dest));

  print_tcp_header(tcp_header);

  if (socket == NULL) {
    printf("tcp socket is NULL; port: %d\n", ntohs(tcp_header->dest));
    return -1;
  }

  packet->socket = socket;

  if (!check_tcp_packet_checksum(packet)) {
    printf("#####tcp checksum is wrong#####\n");

    return -1;
  }

  switch (socket->state) {
    case TCP_SYN_SENT:
      handle_tcp_syn_send(packet);
      break;
    case TCP_ESTABLISHED:
      handle_tcp_established(packet);
      break;
    case TCP_FIN_WAIT_1:
      handle_tcp_fin_wait_1(packet);
      break;
    case TCP_FIN_WAIT_2:
      handle_tcp_fin_wait_2(packet);
      break;
    case TCP_LAST_ACK:
      handle_tcp_last_ack(packet);
      break;
    case TCP_TIME_WAIT:
      handle_tcp_time_wait(packet);
      break;
    default:
      printf("parse_tcp_header: state: %s\n", tcp_state_to_string(socket->state));
      panic("parse_tcp_header: wrong socket state");
  }

  return 0;
}

void
send_tcp_payload(struct socket *socket) {
  while (!socket->rto_mode && send_tcp_segment(socket) != 0);
}

void
tcp_timer(uint32 current_ticks) {
  for (int i = 0; i < SOCKETS_LEN; i++) {
    if (sockets[i] == NULL) {
      continue;
    }

    struct socket *socket = sockets[i];

    if (socket->zwp_active && socket->zwp_next_time == current_ticks) {
      printf("tcp_timer: sent zero window probe\n");

      uint32 send_next = socket->snd_nxt;

      socket->snd_nxt = socket->snd_una;
      send_tcp(socket, TCP_FLAG_ACK, &socket->tx_buf[socket->tx_ack_seq % TCP_SEND_BUF_SIZE], 1);

      socket->snd_nxt = send_next;

      increase_zwp_timer(socket);
    }

    if (socket->rto_active && socket->rto_next_time == current_ticks) {
//      printf("tcp_timer: retransmission\n");

      if (socket->state == TCP_ESTABLISHED) {
        if (socket->snd_nxt != socket->snd_una) {
          socket->rto_mode = 1;
          socket->snd_nxt = socket->snd_una;
          socket->tx_sent_seq = socket->tx_ack_seq;

          send_tcp_segment(socket);
          increase_rto_timer(socket);
        } else {
          stop_rto_timer(socket);
        }
      } else if (socket->state == TCP_SYN_SENT) {
//        printf("tcp_timer: snd_nxt: %d; snd_una: %d\n", socket->snd_nxt, socket->snd_una);

        if (socket->snd_nxt != socket->snd_una) {
          socket->rto_mode = 1;
          socket->snd_nxt = socket->snd_una;

          send_tcp(socket, TCP_FLAG_SYN, NULL, 0);

          increase_rto_timer(socket);
        } else {
          stop_rto_timer(socket);
        }
      }
    }

    if (socket->time_wait_expires == current_ticks) {
      change_state(socket, TCP_CLOSED);
    }
  }
}

static void
handle_tcp_syn_send(struct packet *packet) {
  struct tcp_header *tcp_header = packet->transport_header;
  struct socket *socket = packet->socket;

  if (tcp_header->syn == 1 && tcp_header->ack == 1) {
    if (ntohl(tcp_header->ack_seq) == socket->snd_nxt) {
      socket->snd_una = ntohl(tcp_header->ack_seq);
      socket->snd_wnd = ntohs(tcp_header->window);
      socket->rcv_nxt = ntohl(tcp_header->seq) + 1;
      socket->snd_wnd = ntohs(tcp_header->window);

      change_state(socket, TCP_ESTABLISHED);

      stop_rto_timer(socket);
      send_tcp(socket, TCP_FLAG_ACK, NULL, 0);
      wakeup(socket);
    } else {
      panic("handle_tcp_syn_send: wrong ack_seq - snd_nxt: %d; ");
    }
  }
}

static void
handle_tcp_established(struct packet *packet) {
//  printf("\n");

  struct tcp_header *tcp_header = packet->transport_header;
  struct socket *socket = packet->socket;
  uint32 tcp_payload_len = IPV4_PAYLOAD_LEN(packet) - TCP_HEADER_LEN(tcp_header);

  uint32 seg_seq = ntohl(tcp_header->seq);
//  uint32 acknowledged = ntohl(tcp_header->ack_seq) - socket->snd_una;

  socket->snd_wnd = ntohs(tcp_header->window);

//  printf("handle_tcp_established: received rcv window size: %d\n", socket->snd_wnd);
//  printf("handle_tcp_established: acknowledged: %d\n", acknowledged);

  if (tcp_header->ack == 1 && tcp_header->fin == 1) {
//    printf("handle_tcp_established: server wants to close connection\n");

    socket->rcv_nxt = ntohl(tcp_header->seq) + 1;

    send_tcp(socket, TCP_FLAG_ACK, NULL, 0);
    change_state(socket, TCP_CLOSE_WAIT);

    send_tcp(socket, TCP_FLAG_ACK | TCP_FLAG_FIN, NULL, 0);
    change_state(socket, TCP_LAST_ACK);

    return;
  }

//  printf("handle_tcp_established: tcp payload len: %d\n", tcp_payload_len);

  // add check if ack_seq > snd_una
  if (tcp_header->ack == 1) {
    socket->snd_una = ntohl(tcp_header->ack_seq);
    socket->tx_ack_seq = (socket->snd_una - socket->tx_base_seq);

    if (socket->snd_una == socket->snd_nxt) {
      stop_rto_timer(socket);

      if (socket->snd_wnd == 0) {
        if (!socket->zwp_active) {
          start_zwp_timer(socket);
        }
      } else {
        stop_zwp_timer(socket);
      }
    } else {
      start_rto_timer(socket);
    }
  }

  if (seg_seq == socket->rcv_nxt) {
    socket->rcv_nxt = seg_seq + tcp_payload_len;
  }

  if (tcp_payload_len > 0) {
    send_tcp(socket, TCP_FLAG_ACK, NULL, 0);
  }

//  printf("handle_tcp_established: wait for acknowledgement bytes: %d\n", socket->snd_nxt - socket->snd_una);

  wakeup(socket);
}

static void
handle_tcp_fin_wait_1(struct packet *packet) {
  struct tcp_header *tcp_header = packet->transport_header;
  struct socket *socket = packet->socket;

//  printf("handle_tcp_fin_wait_1: connection closed for server\n");

  if (tcp_header->fin == 1) {
    socket->rcv_nxt = ntohl(tcp_header->seq) + 1;

    send_tcp(socket, TCP_FLAG_ACK, NULL, 0);
    change_state(socket, TCP_TIME_WAIT);
    handle_tcp_time_wait(packet);


    return;
  }

  if (tcp_header->ack == 1) {
    change_state(socket, TCP_FIN_WAIT_2);
  }
}

static void
handle_tcp_fin_wait_2(struct packet *packet) {
  struct tcp_header *tcp_header = packet->transport_header;
  struct socket *socket = packet->socket;

//  printf("handle_tcp_fin_wait_2: connection closed for server; port: %d\n", socket->local_port);

  if (tcp_header->fin == 1) {
    socket->rcv_nxt = ntohl(tcp_header->seq) + 1;

    send_tcp(socket, TCP_FLAG_ACK, NULL, 0);
    change_state(socket, TCP_TIME_WAIT);
    handle_tcp_time_wait(packet);

    return;
  }

  if (tcp_header->ack == 1) {
    socket->snd_una = ntohl(tcp_header->ack_seq);
  }
}

static void
handle_tcp_last_ack(struct packet *packet) {
  struct tcp_header *tcp_header = packet->transport_header;

  if (tcp_header->ack == 1) {
    printf("connection completely closed\n");
  }
}

static void
handle_tcp_time_wait(struct packet *packet) {
  start_time_wait_expires_timer(packet->socket);

  struct tcp_header *tcp_header = packet->transport_header;

  if (tcp_header->fin) {
    // Допустим повторный FIN — отправляем повторный ACK
    return;
  }

  // Проверка: чистый ACK — допустим, просто игнорируем
  if (tcp_header->ack) {
    // Уже всё завершено — игнорируем
    return;
  }
}

void
send_tcp(struct socket *socket, uint8 flags, void *data, int data_len) {
  struct packet *packet = packet_alloc_tx();

  char *payload = packet_push(packet, data_len);
  memmove(payload, data, data_len);

  struct tcp_header *tcp_header = packet->transport_header = packet_push(packet, sizeof(struct tcp_header));

  tcp_header->source = htons(socket->local_port);
  tcp_header->dest = htons(socket->remote_port);
  tcp_header->seq = htonl(socket->snd_nxt);
  tcp_header->ack_seq = htonl(socket->rcv_nxt);

  tcp_header->fin = (flags & TCP_FLAG_FIN) ? 1 : 0;
  tcp_header->syn = (flags & TCP_FLAG_SYN) ? 1 : 0;
  tcp_header->rst = (flags & TCP_FLAG_RST) ? 1 : 0;
  tcp_header->psh = (flags & TCP_FLAG_PSH) ? 1 : 0;
  tcp_header->ack = (flags & TCP_FLAG_ACK) ? 1 : 0;
  tcp_header->urg = (flags & TCP_FLAG_URG) ? 1 : 0;
  tcp_header->ece = (flags & TCP_FLAG_ECE) ? 1 : 0;
  tcp_header->cwr = (flags & TCP_FLAG_CWR) ? 1 : 0;

  tcp_header->doff = 5;
  tcp_header->window = htons(512);
  tcp_header->check = calculate_tcp_checksum(tcp_header, net_dev.ip, socket->address, payload, data_len);

  if (flags & TCP_FLAG_SYN) {
    socket->snd_nxt += 1;
  }

  if (flags & TCP_FLAG_FIN) {
    socket->snd_nxt += 1;
  }

  socket->snd_nxt += data_len;

  send_ipv4(packet, IP_PROTO_TCP, sizeof(struct tcp_header) + data_len, socket->address);

  if (socket->snd_una != socket->snd_nxt && socket->rto_active == 0) {
    start_rto_timer(socket);
  }
}

uint16 static
calculate_tcp_checksum(struct tcp_header *tcp_header, uint32 source_ip, uint32 dest_ip, void *payload,
                       uint32 payload_len) {
  struct pseudo_header pseudo_header;
  pseudo_header.source_ip = htonl(source_ip);
  pseudo_header.dest_ip = htonl(dest_ip);
  pseudo_header.zero = 0;
  pseudo_header.protocol = IP_PROTO_TCP;
  pseudo_header.length = htons(TCP_HEADER_LEN(tcp_header) + payload_len);

  uint32 checksum = 0;
  checksum += calculate_checksum((uint8 *) &pseudo_header, sizeof(struct pseudo_header));
  checksum += calculate_checksum((uint8 *) tcp_header, TCP_HEADER_LEN(tcp_header));

  if (payload != NULL) {
    checksum += calculate_checksum((uint8 *) payload, (int) payload_len);
  }

  return htons(fin_checksum(checksum));
}

static int
check_tcp_packet_checksum(struct packet *packet) {
  struct tcp_header *tcp_header = (struct tcp_header *) packet->transport_header;
  struct ipv4_header *ip_header = (struct ipv4_header *) packet->network_header;
  uint32 payload_len = IPV4_PAYLOAD_LEN(packet);
  uint32 tcp_payload_len = payload_len - TCP_HEADER_LEN(tcp_header);
  void *payload = NULL;

  if (payload_len > 0) {
    payload = ((uint8 *) tcp_header) + TCP_HEADER_LEN(tcp_header);
  }

  uint16 checksum_original = tcp_header->check;
  uint32 saddr = ntohl(ip_header->saddr);
  uint32 daddr = ntohl(ip_header->daddr);

  tcp_header->check = 0;
  uint16 checksum_calculated = calculate_tcp_checksum(tcp_header, saddr, daddr, payload, tcp_payload_len);
  tcp_header->check = checksum_original;

  return checksum_original == checksum_calculated;
}

void
start_rto_timer(struct socket *socket) {
  socket->rto_active = 1;
  socket->rto_count = 0;
  socket->rto_timeout = 20;
  socket->rto_next_time = ticks + socket->rto_timeout;

//  printf("start_rto_timer: start; next tick: %d\n", socket->rto_next_time);
}

static void
increase_rto_timer(struct socket *socket) {
  if (socket->rto_active == 0) {
    panic("increase_rto_timer: timer is not active");
  }

  socket->rto_count++;
  socket->rto_timeout *= 2;
  socket->rto_next_time = ticks + socket->rto_timeout;
}

static void
stop_rto_timer(struct socket *socket) {
  if (socket->rto_active == 0) {
    return;
  }

//  printf("stop_rto_timer: stop\n");

  socket->rto_mode = 0;
  socket->rto_active = 0;
  socket->rto_count = 0;
  socket->rto_timeout = 0;
  socket->rto_next_time = 0;
}

static void
start_zwp_timer(struct socket *socket) {
  socket->zwp_active = 1;
  socket->rto_mode = 0;
  socket->zwp_count = 0;
  socket->zwp_timeout = 20;
  socket->zwp_next_time = ticks + socket->zwp_timeout;
}

static void
increase_zwp_timer(struct socket *socket) {
  if (socket->zwp_active == 0) {
    return;
  }

  socket->zwp_count++;
  socket->zwp_timeout *= 2;
  socket->zwp_next_time = ticks + socket->zwp_timeout;
}

static void
stop_zwp_timer(struct socket *socket) {
  if (socket->zwp_active == 0) {
    return;
  }

  socket->zwp_active = 0;
  socket->zwp_count = 0;
  socket->zwp_timeout = 0;
  socket->zwp_next_time = 0;
}

static void
start_time_wait_expires_timer(struct socket *socket) {
  socket->time_wait_expires = ticks + TIME_WAIT_TIMEOUT_TICKS;
}

static uint32
send_tcp_segment(struct socket *socket) {
  uint8 buffer[MTU] = {0};
  uint32 len, mtu;
  long allowed_window_size = socket->snd_wnd - (socket->snd_nxt - socket->snd_una);

  if (allowed_window_size < 0) {
    panic("send_tcp_segment: allowed_window_size is less then 0");
  }

  mtu = min(MTU, allowed_window_size);

  if (mtu == 0) {
    return 0;
  }

  len = read_available_data(socket, buffer, mtu);

  if (len > 0) {
    send_tcp(socket, TCP_FLAG_ACK, buffer, (int) len);
  }

//  printf("send_tcp_segment: sent: %d\n", len);
//  printf("send_tcp_segment: allowed window size: %d\n", allowed_window_size);

  return len;
}
