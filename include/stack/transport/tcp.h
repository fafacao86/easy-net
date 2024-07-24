#ifndef EASY_NET_TCP_H
#define EASY_NET_TCP_H
#include "net_errors.h"
#include "sock.h"

#pragma pack(1)
typedef struct _tcp_t {
    sock_t base;
}tcp_t;
#pragma pack()


net_err_t tcp_init(void);

sock_t* tcp_create (int family, int protocol);

#endif //EASY_NET_TCP_H
