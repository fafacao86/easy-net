#ifndef EASY_NET_TCP_H
#define EASY_NET_TCP_H
#include "net_errors.h"
#include "sock.h"
#include "log.h"
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

#pragma pack(1)
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


typedef struct _tcp_t {
    sock_t base;
    struct {
        sock_wait_t wait;       // wait_t for connection establishment
    }conn;
} tcp_t;


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
void tcp_seg_init (tcp_seg_t * seg, packet_t * buf, ipaddr_t * local, ipaddr_t * remote);
void tcp_set_hdr_size (tcp_hdr_t * hdr, int size);
int tcp_hdr_size (tcp_hdr_t * hdr);


net_err_t tcp_close(struct _sock_t* sock);
net_err_t tcp_connect(struct _sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len);

#endif //EASY_NET_TCP_H
