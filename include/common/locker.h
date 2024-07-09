#ifndef EASY_NET_LOCKER_H
#define EASY_NET_LOCKER_H
#include "net_plat.h"
#include "sys_plat.h"


typedef enum locker_type_t {
    LOCKER_NONE,
    LOCKER_THREAD,
    LOCKER_INT,
}locker_type_t;

typedef struct locker_t {
    locker_type_t type;
    union {
        sys_mutex_t mutex;
#if NETIF_USE_INT == 1
        sys_intlocker_t state;
#endif
    };
}locker_t;

net_err_t locker_init(locker_t * locker, locker_type_t type);
void locker_destroy(locker_t * locker);
void locker_lock(locker_t * locker);
void locker_unlock(locker_t * locker);
#endif //EASY_NET_LOCKER_H
