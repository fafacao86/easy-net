#ifndef EASY_NET_UDP_H
#define EASY_NET_UDP_H
#include "sock.h"
#include "socket.h"

typedef struct _udp_t {
    sock_t  base;                   // base class
    list_t recv_list;
    sock_wait_t rcv_wait;
}udp_t;

net_err_t udp_init(void);
sock_t* udp_create(int family, int protocol);
#endif //EASY_NET_UDP_H
