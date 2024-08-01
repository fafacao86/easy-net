#ifndef EASY_NET_TCP_OUT_H
#define EASY_NET_TCP_OUT_H
#include "tcp.h"

typedef enum _tcp_oevent_t {
    TCP_OEVENT_SEND,            // new send call() event
    TCP_OEVENT_XMIT,            // transmit buffered data event
    TCP_OEVENT_TMO,
}tcp_oevent_t;

net_err_t tcp_send_reset(tcp_seg_t * seg);
net_err_t tcp_transmit(tcp_t * tcp);
net_err_t tcp_send_syn(tcp_t* tcp);
net_err_t tcp_send_ack(tcp_t* tcp, tcp_seg_t * seg);
net_err_t tcp_ack_process (tcp_t * tcp, tcp_seg_t * seg);
net_err_t tcp_send_fin (tcp_t* tcp);
int tcp_write_sndbuf(tcp_t * tcp, const uint8_t * buf, int len);
net_err_t tcp_send_reset_for_tcp(tcp_t* tcp);
net_err_t tcp_send_keepalive(tcp_t* tcp);
#endif //EASY_NET_TCP_OUT_H
