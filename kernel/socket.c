#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"
#include "socket.h"

int socketalloc(struct file *file, int type) {
  if (type != SOCK_STREAM && type != SOCK_DGRAM) {
    return -1;
  }

  struct socket *socket = kalloc();

  if (socket == NULL) {
    return -1;
  }

  memset(socket, 0, PGSIZE);

  socket->tx_buf = kalloc();

  if (socket->tx_buf == NULL) {
    return -1;
  }

  memset(socket->tx_buf, 0, PGSIZE);

  initlock(&socket->lock, "socket_lock");
  initlock(&socket->rx_lock, "socket_rx");
  initlock(&socket->tx_lock, "socket_tx");

  file->socket = socket;
  file->type = FD_SOCKET;
  file->socket->type = type;

  return 0;
}

int socketconnect(struct file *file, uint16 remote_port, int address) {
  if (allocate_port(file->socket) == -1) {
    return -1;
  }

  printf("socket created with a local port: %d\n", file->socket->local_port);

  acquire(&file->socket->lock);

  file->socket->remote_port = remote_port;
  file->socket->address = address;

  release(&file->socket->lock);

  if (file->socket->type == SOCK_STREAM) {
    if (connect_tcp(file->socket) == -1) {
      return -1;
    }

    acquire(&file->socket->lock);

    while (file->socket->state == TCP_SYN_SENT) {
      printf("socketconnect: sleeping...\n");
      sleep(file->socket, &file->socket->lock);
    }

    file->socket->tx_base_seq = file->socket->snd_una;

    release(&file->socket->lock);

    if (file->socket->state != TCP_ESTABLISHED) {
      return -1;
    }
  }

  return 0;
}

int
write_send_data(struct socket *socket, uint64 src, uint32 max_len) {
  int len = 0;

  for (int i = 0; i < max_len; i++) {
    uint next = (socket->tx_nwrite + 1) % TCP_SEND_BUF_SIZE;

    if (next == socket->tx_ack_seq % TCP_SEND_BUF_SIZE) {
      return len;
    }

    char c;
    copyin(myproc()->pagetable, &c, src + i, 1);

    socket->tx_buf[socket->tx_nwrite % TCP_SEND_BUF_SIZE] = c;
    socket->tx_nwrite++;
    len++;
  }

  return len;
}

int
read_available_data(struct socket *socket, uint8 *dst, uint32 max_len) {
  int len = 0;

  while (socket->tx_sent_seq % TCP_SEND_BUF_SIZE != socket->tx_nwrite % TCP_SEND_BUF_SIZE && len < max_len) {
    dst[len++] = socket->tx_buf[socket->tx_sent_seq++ % TCP_SEND_BUF_SIZE];
  }

  return len;
}

int
socketwrite(struct file *file, uint64 addr, int len) {
  if (file->socket->type == SOCK_STREAM) {
    acquire(&file->socket->lock);

    int added = 0;

    while (1) {
      added += write_send_data(file->socket, addr + added, len - added);

      if (added != len) {
//        printf("write_send_data: buffer is full: sleeping...\n");
//        printf("write_send_data: is not sent yet: %d...\n", file->socket->tx_nwrite + 1 - file->socket->tx_ack_seq);

        send_tcp_payload(file->socket);
        sleep(file->socket, &file->socket->lock);

        continue;
      }

      break;
    }

//    printf("write_send_data: sent: %d...\n", len);

    send_tcp_payload(file->socket);
    release(&file->socket->lock);
  } else if (file->socket->type == SOCK_DGRAM) {
    struct packet *packet = packet_alloc_tx();

    packet_push(packet, len);
    copyin(myproc()->pagetable, packet->buffer, addr, len);

    packet->socket = file->socket;

    send_udp(packet, len, file->socket->local_port, file->socket->remote_port, file->socket->address);

    kfree(packet);
  } else {
    panic("socketwrite: unexpected socket type");
  }

  return len;
}

int socketbind(struct file *file, uint16 local_port) {
  file->socket->local_port = local_port;

  return add_socket(file->socket);
}

int socketread(struct file *file, uint64 addr, int len) {
  struct socket *socket = file->socket;

  acquire(&socket->rx_lock);

  while (!socket->rx_head) {
    sleep(socket, &socket->rx_lock);
  }

  struct rx_buffer *buffer = socket->rx_head;
  int copy_len = len > buffer->len ? buffer->len : len;

  socket->rx_head = buffer->next;

  if (!socket->rx_head) {
    socket->rx_tail = NULL;
  }

  socket->rx_queue_len--;

  if (copyout(myproc()->pagetable, addr, buffer->data, copy_len) != 0) {
    release(&socket->rx_lock);

    return -1;
  }

  release(&socket->rx_lock);

  kfree(buffer);

  return copy_len;
}

int socketclose(struct socket *socket) {
  if (socket->type == SOCK_STREAM) {
    close_tcp(socket);
  }

//  release_socket(socket);
//  kfree((void *) socket);

  return 0;
}

int socket_add_to_rx_queue(struct socket *socket, void *data, int len) {
  struct rx_buffer *buffer = kalloc();

  if (buffer == NULL) {
    panic("rx_buffer alloc");
  }

  memset(buffer, 0, PGSIZE);
  buffer->data = ((uint8 *) buffer) + sizeof(struct rx_buffer);
  buffer->len = len;
  memmove(buffer->data, (uint8 *) data, len);

  acquire(&socket->rx_lock);

  if (socket->rx_queue_len == RX_BUFFER_QUEUE_SIZE) {
    release(&socket->rx_lock);

    return -1;
  }

  if (socket->rx_tail) {
    socket->rx_tail->next = buffer;
  } else {
    socket->rx_head = buffer;
  }

  socket->rx_tail = buffer;
  socket->rx_queue_len++;

  release(&socket->rx_lock);

  return 0;
}
