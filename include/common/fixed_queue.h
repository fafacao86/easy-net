#ifndef EASY_NET_FIXED_QUEUE_H
#define EASY_NET_FIXED_QUEUE_H
#include "list.h"
#include "locker.h"
#include "net_plat.h"


typedef struct fixq_t{
    int size;
    void** buffer;
    int in;
    int out;
    int cnt;
    locker_t locker;
    sys_sem_t recv_sem;
    sys_sem_t send_sem;
}fixed_queue_t;

net_err_t fixed_queue_init(fixed_queue_t * q, void ** buf, int size, locker_type_t share_type);

net_err_t fixed_queue_send(fixed_queue_t* q, void* msg, int tmo);

void * fixed_queue_recv(fixed_queue_t* q, int tmo);

void fixed_queue_destroy(fixed_queue_t * q);

int fixed_queue__count (fixed_queue_t *q);

#endif //EASY_NET_FIXED_QUEUE_H
