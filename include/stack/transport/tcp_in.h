#ifndef EASY_NET_TCP_IN_H
#define EASY_NET_TCP_IN_H

#include "tcp.h"

net_err_t tcp_in(packet_t *buf, ipaddr_t *src_ip, ipaddr_t *dest_ip);

net_err_t tcp_data_in (tcp_t * tcp, tcp_seg_t * seg);
void tcp_read_options(tcp_t *tcp, tcp_hdr_t * tcp_hdr);

#endif //EASY_NET_TCP_IN_H
