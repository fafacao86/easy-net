#ifndef EASY_NET_RAW_H
#define EASY_NET_RAW_H
#include "sock.h"

typedef struct _raw_t {
    sock_t base;
    sock_wait_t rcv_wait;
}raw_t;

net_err_t raw_init(void);
sock_t* raw_create(int family, int protocol);
net_err_t raw_in(packet_t* packet);
#endif //EASY_NET_RAW_H
