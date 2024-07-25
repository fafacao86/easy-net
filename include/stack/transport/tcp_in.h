#ifndef EASY_NET_TCP_IN_H
#define EASY_NET_TCP_IN_H

#include "tcp.h"

net_err_t tcp_in(packet_t *buf, ipaddr_t *src_ip, ipaddr_t *dest_ip);

#endif //EASY_NET_TCP_IN_H
