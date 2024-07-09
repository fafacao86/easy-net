#include "fixed_queue.h"
#include "locker.h"
#include "log.h"
#include "sys_plat.h"
#include "easy_net_config.h"

net_err_t fixed_queue_init(fixed_queue_t *q, void **buf, int size, locker_type_t type) {
    q->size = size;
    q->in = q->out = q->cnt = 0;
    q->buffer = (void **)0;
    q->recv_sem = SYS_SEM_INVALID;
    q->send_sem = SYS_SEM_INVALID;
    net_err_t err = locker_init(&q->locker, type);
    if (err < 0) {
        log_error(LOG_QUEUE, "init locker failed!");
        return err;
    }
    q->send_sem = sys_sem_create(size);
    if (q->send_sem == SYS_SEM_INVALID)  {
        log_error(LOG_QUEUE, "create sem failed!");
        err = NET_ERR_SYS;
        goto init_failed;
    }
    q->recv_sem = sys_sem_create(0);
    if (q->recv_sem == SYS_SEM_INVALID) {
        log_error(LOG_QUEUE, "create sem failed!");
        err = NET_ERR_SYS;
        goto init_failed;
    }
    q->buffer = buf;
    return NET_OK;
    init_failed:
    if (q->send_sem != SYS_SEM_INVALID) {
        sys_sem_free(q->send_sem);
    }
    locker_destroy(&q->locker);
    return err;
}


net_err_t fixed_queue_send(fixed_queue_t *q, void *msg, int tmo) {
    locker_lock(&q->locker);
    if ((q->cnt >= q->size) && (tmo < 0)) {
        locker_unlock(&q->locker);
        return NET_ERR_FULL;
    }
    locker_unlock(&q->locker);

    if (sys_sem_wait(q->send_sem, tmo) < 0) {
        return NET_ERR_TIMEOUT;
    }

    locker_lock(&q->locker);
    q->buffer[q->in++] = msg;
    if (q->in >= q->size) {
        q->in = 0;
    }
    q->cnt++;
    locker_unlock(&q->locker);

    sys_sem_notify(q->recv_sem);
    return NET_OK;
}

void *fixed_queue_recv(fixed_queue_t *q, int tmo) {
    locker_lock(&q->locker);
    if (!q->cnt && (tmo < 0)) {
        locker_unlock(&q->locker);
        return (void *)0;
    }
    locker_unlock(&q->locker);

    if (sys_sem_wait(q->recv_sem, tmo) < 0) {
        return (void *)0;
    }

    locker_lock(&q->locker);
    void *msg = q->buffer[q->out++];
    if (q->out >= q->size) {
        q->out = 0;
    }
    q->cnt--;
    locker_unlock(&q->locker);

    sys_sem_notify(q->send_sem);
    return msg;
}

void fixed_queue_destroy(fixed_queue_t *q) {
    locker_destroy(&q->locker);
    sys_sem_free(q->send_sem);
    sys_sem_free(q->recv_sem);
}

int fixed_queue__count (fixed_queue_t *q) {
    locker_lock(&q->locker);
    int count = q->cnt;
    locker_unlock(&q->locker);
    return count;
}
