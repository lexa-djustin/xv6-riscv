#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"
#include "net.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0_NET + (r)))

int dropped_packets = 0;

enum transmit_status {
    TRANSMIT_WAITING, TRANSMIT_DONE, TRANSMIT_NO_WAIT
};

struct virtio_net_queue {
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
    char is_free[NUM];
    enum transmit_status is_transmit_done[NUM];
    uint16 used_idx;
};

struct dev {
    struct virtio_net_queue rx_queue;
    struct virtio_net_queue tx_queue;
} dev;

void
virtq_queue_init(struct virtio_net_queue *vq, int queue_index) {
  *R(VIRTIO_MMIO_QUEUE_SEL) = queue_index;

  if (*R(VIRTIO_MMIO_QUEUE_READY)) {
    panic("virtio net should not be ready");
  }

  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);

  if (max == 0) {
    panic("virtio net has no queue 0");
  }
  if (max < NUM) {
    panic("virtio net max queue too short");
  }

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  vq->desc = kalloc();
  vq->avail = kalloc();
  vq->used = kalloc();

  if (!vq->desc || !vq->avail || !vq->used) {
    panic("virtio net kalloc");
  }

  memset(vq->desc, 0, PGSIZE);
  memset(vq->avail, 0, PGSIZE);
  memset(vq->used, 0, PGSIZE);

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64) vq->desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64) vq->desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64) vq->avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64) vq->avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64) vq->used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64) vq->used >> 32;

  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  for (int i = 0; i < NUM; i++) {
    vq->is_free[i] = 1;
  }

//  init_packet_queue();
}

void
virtio_net_init_rx_buffers(struct virtio_net_queue *rx_queue) {
  for (int i = 0; i < NUM; i++) {
    struct virtq_desc *desc = &rx_queue->desc[i];
    void *buf = kalloc();

    if (!buf) {
      panic("Failed to allocate RX buffer");
    }

    memset(buf, 0, PGSIZE);

    desc->addr = (uint64) buf;
    desc->len = PGSIZE;
    desc->flags = VRING_DESC_F_WRITE;
    desc->next = 0;

    rx_queue->avail->ring[rx_queue->avail->idx++ % NUM] = i;
  }
}

void
virtio_net_init(void) {
  uint32 status = 0;

  initlock(&net_dev.lock, "virtio_net");
  init_rx_packet_queue(&netq);

  dev.rx_queue.used_idx = 0;
  dev.tx_queue.used_idx = 0;

  uint8 mac[MAC_ADDRESS_LENGTH] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
  memmove(net_dev.mac, mac, MAC_ADDRESS_LENGTH);

  net_dev.ip = 0xC1A80101;
  net_dev.dev = &dev;

  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(VIRTIO_MMIO_VERSION) != 2 ||
      *R(VIRTIO_MMIO_DEVICE_ID) != 1 ||
      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("could not find virtio net");
  }

  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  *R(VIRTIO_MMIO_DRIVER_FEATURES) = VIRTIO_NET_F_MAC;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);

  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio net FEATURES_OK unset");

  virtq_queue_init(&dev.rx_queue, VIRTIO_NET_QUEUE_RX);
  virtq_queue_init(&dev.tx_queue, VIRTIO_NET_QUEUE_TX);

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  virtio_net_init_rx_buffers(&dev.rx_queue);

  printf("virtio-net: initialized\n");
}

static int
alloc_desc(struct virtio_net_queue *queue) {
  for (int i = 0; i < NUM; i++) {
    if (queue->is_free[i]) {
      queue->is_free[i] = 0;

      return i;
    }
  }

  return -1;
}

static void
free_desc(struct virtio_net_queue *queue, uint32 i) {
  if (NUM <= i) {
    panic("free_desc: i too large");
  }

  if (queue->is_free[i]) {
    panic("free_desc: desc already free");
  }

  queue->is_free[i] = 1;
  queue->is_transmit_done[i] = TRANSMIT_WAITING;
}

void
virtio_write_packet(struct packet *packet) {
  acquire(&net_dev.lock);

  struct virtio_net_queue *tx_queue = &dev.tx_queue;
  int idx = alloc_desc(tx_queue);

  while (idx == -1) {
    idx = alloc_desc(tx_queue);
  }

  struct virtq_desc *desc = &tx_queue->desc[idx];

  desc->addr = (uint64) packet->buffer;
  desc->len = packet->len;
  desc->flags = 0;
  desc->next = 0;

  tx_queue->avail->ring[tx_queue->avail->idx % NUM] = idx;
  __sync_synchronize();
  tx_queue->avail->idx++;
  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = VIRTIO_NET_QUEUE_TX;

  tx_queue->is_transmit_done[idx] = TRANSMIT_NO_WAIT;

  release(&net_dev.lock);
}

void
virtio_net_handle_rx_interrupt() {
  acquire(&net_dev.lock);

  struct virtio_net_queue *tx_queue = &dev.tx_queue;

  while (tx_queue->used_idx != tx_queue->used->idx) {
    uint32 desc_id = tx_queue->used->ring[tx_queue->used_idx % NUM].id;
    tx_queue->used_idx++;

    free_desc(tx_queue, desc_id);
  }

  struct virtio_net_queue *rx_queue = &dev.rx_queue;

  while (rx_queue->used_idx != rx_queue->used->idx) {
    uint32 desc_id = rx_queue->used->ring[rx_queue->used_idx % NUM].id;
    uint32 len = rx_queue->used->ring[rx_queue->used_idx % NUM].len;
    struct virtq_desc *desc = &rx_queue->desc[desc_id];

    struct packet *packet = packet_alloc_rx();
    packet->len = len;

    memmove(packet->buffer, (void *) desc->addr, len);

//    parse_ethernet_frame(packet);

    if (enqueue_packet((void *) packet) == -1) {
      dropped_packets++;
      printf("Dropped packets: %d\n", dropped_packets);
    }

    rx_queue->avail->ring[rx_queue->avail->idx % NUM] = desc_id;

    __sync_synchronize();
    rx_queue->avail->idx++;
    __sync_synchronize();

    rx_queue->used_idx++;
  }

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = VIRTIO_NET_QUEUE_RX;

  release(&net_dev.lock);
}
