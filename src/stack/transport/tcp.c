#include "tcp.h"
#include "memory_pool.h"
#include "log.h"


static tcp_t tcp_tbl[TCP_MAX_NR];
static memory_pool_t tcp_mblock;
static list_t tcp_list;


/**
 * allocate a new tcp socket, if all used, reuse those in time_wait state
 */
static tcp_t * tcp_get_free (int wait) {
    tcp_t* tcp = (tcp_t*)memory_pool_alloc(&tcp_mblock, wait ? 0 : -1);
    if (!tcp) {
        return (tcp_t *)0;
    }
    return tcp;
}


static tcp_t* tcp_alloc(int wait, int family, int protocol) {
    static const sock_ops_t tcp_ops;
    tcp_t* tcp = tcp_get_free(wait);
    if (!tcp) {
        log_error(LOG_TCP, "no tcp sock");
        return (tcp_t*)0;
    }
    plat_memset(tcp, 0, sizeof(tcp_t));
    net_err_t err = sock_init((sock_t*)tcp, family, protocol, &tcp_ops);
    if (err < 0) {
        log_error(LOG_TCP, "create failed.");
        memory_pool_free(&tcp_mblock, tcp);
        return (tcp_t*)0;
    }
    return tcp;
}

void tcp_insert (tcp_t * tcp) {
    list_insert_last(&tcp_list, &tcp->base.node);
    assert_halt(tcp_list.count <= TCP_MAX_NR, "tcp free");
}


sock_t* tcp_create (int family, int protocol) {
    tcp_t* tcp = tcp_alloc(1, family, protocol);
    if (!tcp) {
        log_error(LOG_TCP, "alloc tcp failed.");
        return (sock_t *)0;
    }
    tcp_insert(tcp);
    return (sock_t *)tcp;
}

net_err_t tcp_init(void) {
    log_info(LOG_TCP, "tcp init.");
    memory_pool_init(&tcp_mblock, tcp_tbl, sizeof(tcp_t), TCP_MAX_NR, LOCKER_NONE);
    init_list(&tcp_list);
    log_info(LOG_TCP, "init done.");
    return NET_OK;
}
