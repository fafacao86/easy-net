#ifndef EASY_NET_RAW_H
#define EASY_NET_RAW_H
#include "sock.h"

typedef struct _raw_t {
    sock_t base;
}raw_t;

net_err_t raw_init(void);
sock_t* raw_create(int family, int protocol);

#endif //EASY_NET_RAW_H
