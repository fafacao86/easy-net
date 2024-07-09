#include "locker.h"

net_err_t locker_init(locker_t * locker, locker_type_t type) {
    if (type == LOCKER_THREAD) {
        sys_mutex_t mutex = sys_mutex_create();
        if (mutex == SYS_MUTEx_INVALID) {
            return NET_ERR_SYS;
        }
        locker->mutex = mutex;
    }

    locker->type = type;
    return NET_OK;
}

void locker_destroy(locker_t * locker) {
    if (locker->type == LOCKER_THREAD) {
        sys_mutex_free(locker->mutex);
    }
}

void locker_lock(locker_t * locker) {
    if (locker->type == LOCKER_THREAD) {
        sys_mutex_lock(locker->mutex);
    }
}

void locker_unlock(locker_t * locker) {
    if (locker->type == LOCKER_THREAD) {
        sys_mutex_unlock(locker->mutex);
    } else if (locker->type ==LOCKER_INT) {
#if NETIF_USE_INT
        sys_intlocker_unlock(locker->state);
#endif
    }
}