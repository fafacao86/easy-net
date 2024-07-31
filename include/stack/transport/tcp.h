#ifndef EASY_NET_TCP_H
#define EASY_NET_TCP_H
#include "net_errors.h"
#include "sock.h"
#include "log.h"
#include "tcp_buf.h"
#include "timer.h"
/**
 * https://datatracker.ietf.org/doc/html/rfc793
 *
 *                               +---------+ ---------\      active OPEN
                              |  CLOSED |            \    -----------
                              +---------+<---------\   \   create TCB
                                |     ^              \   \  snd SYN
                   passive OPEN |     |   CLOSE        \   \
                   ------------ |     | ----------       \   \
                    create TCB  |     | delete TCB         \   \
                                V     |                      \   \
                              +---------+            CLOSE    |    \
                              |  LISTEN |          ---------- |     |
                              +---------+          delete TCB |     |
                   rcv SYN      |     |     SEND              |     |
                  -----------   |     |    -------            |     V
 +---------+      snd SYN,ACK  /       \   snd SYN          +---------+
 |         |<-----------------           ------------------>|         |
 |   SYN   |                    rcv SYN                     |   SYN   |
 |   RCVD  |<-----------------------------------------------|   SENT  |
 |         |                    snd ACK                     |         |
 |         |------------------           -------------------|         |
 +---------+   rcv ACK of SYN  \       /  rcv SYN,ACK       +---------+
   |           --------------   |     |   -----------
   |                  x         |     |     snd ACK
   |                            V     V
   |  CLOSE                   +---------+
   | -------                  |  ESTAB  |
   | snd FIN                  +---------+
   |                   CLOSE    |     |    rcv FIN
   V                  -------   |     |    -------
 +---------+          snd FIN  /       \   snd ACK          +---------+
 |  FIN    |<-----------------           ------------------>|  CLOSE  |
 | WAIT-1  |------------------                              |   WAIT  |
 +---------+          rcv FIN  \                            +---------+
   | rcv ACK of FIN   -------   |                            CLOSE  |
   | --------------   snd ACK   |                           ------- |
   V        x                   V                           snd FIN V
 +---------+                  +---------+                   +---------+
 |FINWAIT-2|                  | CLOSING |                   | LAST-ACK|
 +---------+                  +---------+                   +---------+
   |                rcv ACK of FIN |                 rcv ACK of FIN |
   |  rcv FIN       -------------- |    Timeout=2MSL -------------- |
   |  -------              x       V    ------------        x       V
    \ snd ACK                 +---------+delete TCB         +---------+
     ------------------------>|TIME WAIT|------------------>| CLOSED  |
                              +---------+                   +---------+
 *
 * */

#define TCP_OPT_END        0
#define TCP_OPT_NOP        1
#define TCP_OPT_MSS        2

#pragma pack(1)
typedef struct _tcp_opt_mss_t {
    uint8_t kind;
    uint8_t length;
    union {
        uint16_t mss;
    };
}tcp_opt_mss_t;


typedef struct _tcp_hdr_t {
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    union {
        uint16_t flags;
#if NET_ENDIAN_LITTLE
        struct {
            uint16_t resv : 4;          // reserved
            uint16_t shdr : 4;          // header length, unit is 4 bytes
            uint16_t f_fin : 1;           // FIN
            uint16_t f_syn : 1;           // SYN
            uint16_t f_rst : 1;           // RESET
            uint16_t f_psh : 1;           // PUSH
            uint16_t f_ack : 1;           // ACK
            uint16_t f_urg : 1;           // URGENT
            uint16_t f_ece : 1;           // EARLY CONGESTION EXPERIENCE
            uint16_t f_cwr : 1;           // CONGESTION WINDOW REDUCED
        };
#else
        struct {
            uint16_t shdr : 4;
            uint16_t resv : 4;
            uint16_t f_cwr : 1;
            uint16_t f_ece : 1;
            uint16_t f_urg : 1;
            uint16_t f_ack : 1;
            uint16_t f_psh : 1;
            uint16_t f_rst : 1;
            uint16_t f_syn : 1;
            uint16_t f_fin : 1;
        };
#endif
    };
    uint16_t win;                       // window size, can be used with window scaling
    uint16_t checksum;
    uint16_t urgptr;                    // urgent pointer
}tcp_hdr_t;

typedef struct _tcp_pkt_t {
    tcp_hdr_t hdr;
    uint8_t data[1];
}tcp_pkt_t;

/**
 * tcp segment structure
 * */
typedef struct _tcp_seg_t {
    ipaddr_t local_ip;
    ipaddr_t remote_ip;
    tcp_hdr_t * hdr;
    packet_t * buf;
    uint32_t data_len;
    uint32_t seq;
    uint32_t seq_len;
}tcp_seg_t;
#pragma pack()


typedef enum _tcp_state_t {
    TCP_STATE_FREE = 0,             // not in official state list
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,

    TCP_STATE_MAX,      // not a state, used to get the number of states
}tcp_state_t;



typedef struct _tcp_t {
    sock_t base;
    struct _tcp_t * parent;
    struct {
        sock_wait_t wait;       // wait_t for connection establishment
        int keep_idle;
        int keep_intvl;
        int keep_cnt;         // number of keepalive retries
        int keep_retry;         // current remaining retries
        net_timer_t keep_timer; // timer for keepalive and retry
        int backlog;            // full-connection queue size
    } conn;
    int mss;

    // these flags are used to assist keep track of the state of the tcp connection
    struct {
        uint32_t syn_out: 1;        // need to send SYN
        uint32_t fin_out: 1;        // need to send FIN
        uint32_t irs_valid: 1;      // this is to denote that we have set the recv window variables
        uint32_t fin_in: 1;         // received FIN, this means we have received all the data and the FIN
        uint32_t keep_enable : 1;     // keep-alive enabled
        uint32_t inactive : 1;      // this is to determine if the tcb is accepted by user
    } flags;


    // checkout RFC 793 Figure 4
    struct {
        tcp_buf_t buf;      // send buffer
        uint8_t  data[TCP_SBUF_SIZE];
        uint32_t una;	    // send unacknowledged
        uint32_t nxt;	    // seq that hasn't been sent yet
        uint32_t iss;	    // initial send sequence number
        sock_wait_t wait;   // send wait structure
    } snd;

    // checkout RFC 793 Figure 5
    struct {
        tcp_buf_t buf;      // receive buffer
        uint8_t  data[TCP_RBUF_SIZE];
        uint32_t nxt;	    // the seq number of the next expected packet
        uint32_t iss;	    // initial receive sequence number
        sock_wait_t wait;   // rcv wait structure
    } rcv;

    tcp_state_t state;
} tcp_t;


int inline tcp_rcv_window (tcp_t * tcp) {
    int window = tcp_buf_free_cnt(&tcp->rcv.buf);
    return window;
}


#if LOG_DISP_ENABLED(LOG_TCP)
void tcp_show_info (char * msg, tcp_t * tcp);
void tcp_display_pkt (char * msg, tcp_hdr_t * tcp_hdr, packet_t * buf);
void tcp_show_list (void);
#else
#define tcp_show_info(msg, tcp)
#define tcp_display_pkt(msg, hdr, buf)
#define tcp_show_list()
#endif


net_err_t tcp_init(void);
sock_t* tcp_create (int family, int protocol);
sock_t* tcp_find(ipaddr_t * local_ip, uint16_t local_port, ipaddr_t * remote_ip, uint16_t remote_port);
void tcp_seg_init (tcp_seg_t * seg, packet_t * buf, ipaddr_t * local, ipaddr_t * remote);
void tcp_set_hdr_size (tcp_hdr_t * hdr, int size);
int tcp_hdr_size (tcp_hdr_t * hdr);


net_err_t tcp_close(struct _sock_t* sock);
net_err_t tcp_connect(struct _sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len);
net_err_t tcp_abort (tcp_t * tcp, int err);
net_err_t tcp_send (struct _sock_t* sock, const void* buf, size_t len, int flags, ssize_t * result_len);
net_err_t tcp_recv (struct _sock_t* s, void* buf, size_t len, int flags, ssize_t * result_len);
net_err_t tcp_listen (struct _sock_t* s, int backlog);
net_err_t tcp_accept (struct _sock_t *s, struct x_sockaddr* addr, x_socklen_t* len, struct _sock_t ** client);

void tcp_keepalive_start (tcp_t * tcp, int run);
void tcp_keepalive_restart (tcp_t * tcp);
void tcp_kill_all_timers (tcp_t * tcp);
int tcp_backlog_count (tcp_t * tcp);
tcp_t * tcp_create_child (tcp_t * parent, tcp_seg_t * seg);

#define TCP_SEQ_LE(a, b)        ((int32_t)(a) - (int32_t)(b) <= 0)
#define TCP_SEQ_LT(a, b)        ((int32_t)(a) - (int32_t)(b) < 0)


#endif //EASY_NET_TCP_H
