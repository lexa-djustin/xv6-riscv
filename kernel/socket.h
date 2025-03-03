#define RX_BUFFER_QUEUE_SIZE 64
#define TCP_SEND_BUF_SIZE 2048
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

struct rx_buffer {
    struct rx_buffer *next;
    int len;
    void *data;
};

enum socket_tcp_state {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
};

struct socket {
    uint16 local_port;
    uint16 remote_port;
    int address;

    struct rx_buffer *rx_head;
    struct rx_buffer *rx_tail;
    int rx_queue_len;
    struct spinlock rx_lock;
    struct spinlock tx_lock;

    int type;
    enum socket_tcp_state state;
    struct spinlock lock;

    // Отправка
    uint32 snd_una;   // самый ранний неподтверждённый SEQ
    uint32 snd_nxt;   // следующий SEQ для отправки
    uint32 snd_wnd;   // размер окна, который дал получатель

    // Приём
    uint32 rcv_nxt;   // следующий ожидаемый байт от удалённого узла
    uint32 rcv_wnd;   // размер нашего окна

    // struct send_buffer
    char *tx_buf;
    uint tx_base_seq;
    uint tx_sent_seq;
    uint tx_ack_seq;
    uint tx_nwrite;

    // Zero Window Probe timer
    uint32 zwp_next_time;   // системное время следующей попытки
    uint32 zwp_timeout;   // текущий интервал (экспоненциальный)
    uint8 zwp_count;   // сколько ZWP уже отправлено
    int zwp_active;

    // Retransmission timer
    uint8 rto_mode;
    uint32 rto_next_time;
    uint32 rto_timeout;
    uint8 rto_count;
    int rto_active;

    // Time wait state timer
    uint64 time_wait_expires;
};
