#include "udp.h"
#include "memory_pool.h"
#include "log.h"
#include "utils.h"

static udp_t udp_tbl[UDP_MAX_NR];
static memory_pool_t udp_mblock;
static list_t udp_list;

net_err_t udp_init(void) {
    log_info(LOG_UDP, "udp init.");
    memory_pool_init(&udp_mblock, udp_tbl, sizeof(udp_t), UDP_MAX_NR, LOCKER_NONE);
    init_list(&udp_list);

    log_info(LOG_UDP, "init done.");
    return NET_OK;
}


static int is_port_used(int port) {
    list_node_t * node;

    list_for_each(node, &udp_list) {
        sock_t* sock = list_entry(node, sock_t, node);
        if (sock->local_port == port) {
            return 1;
        }
    }

    return 0;
}

static net_err_t alloc_port(sock_t* sock) {
    static int search_index = NET_PORT_DYN_START;
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; i++) {
        int port = search_index++;
        if (!is_port_used(port)) {
            sock->local_port = port;
            return NET_OK;
        }
    }
    return NET_ERR_NONE;
}



/**
 * API consumer may not specify the local port, we need to allocate one.
 * */
net_err_t udp_sendto (struct _sock_t * sock, const void* buf, size_t len, int flags, const struct x_sockaddr* dest,
                      x_socklen_t dest_len, ssize_t * result_len) {
    struct x_sockaddr_in * addr = (struct x_sockaddr_in*)dest;
    // check if the socket is connected,
    // if connected, check if the dest addr and port is the same as the connected one.
    ipaddr_t dest_ip;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    uint16_t dport = e_ntohs(addr->sin_port);

    if (!ipaddr_is_any(&sock->remote_ip)) {
        if (!ipaddr_is_equal(&dest_ip, &sock->remote_ip) || (dport != sock->remote_port)) {
            log_error(LOG_UDP, "udp is connected");
            return NET_ERR_CONNECTED;
        }
    }

    // there might be no local port bound yet, allocate one
    if (!sock->local_port && ((sock->err = alloc_port(sock)) < 0)) {
        log_error(LOG_UDP, "no port avaliable");
        return NET_ERR_NONE;
    }

    packet_t* pktbuf = packet_alloc((int)len);
    if (!pktbuf) {
        log_error(LOG_UDP, "no buffer");
        return NET_ERR_MEM;
    }

    net_err_t err = packet_write(pktbuf, (uint8_t*)buf, (int)len);
    if (err < 0) {
        log_error(LOG_UDP, "copy data error");
        goto end_sendto;
    }


    if (result_len) {
        *result_len = (ssize_t)len;
    }
    return NET_OK;
    end_sendto:
    packet_free(pktbuf);
    return err;
}



sock_t* udp_create(int family, int protocol) {
    static const sock_ops_t udp_ops = {
            .setopt = sock_setopt,
            .sendto = udp_sendto,
    };
    udp_t* udp = (udp_t *)memory_pool_alloc(&udp_mblock, 0);
    if (!udp) {
        log_error(LOG_UDP, "no sock");
        return (sock_t*)0;
    }

    net_err_t err = sock_init((sock_t*)udp, family, protocol, &udp_ops);
    if (err < 0) {
        log_error(LOG_UDP, "create failed.");
        memory_pool_free(&udp_mblock, udp);
        return (sock_t*)0;
    }
    init_list(&udp->recv_list);
    // only recv might needs to wait in udp
    udp->base.rcv_wait = &udp->rcv_wait;
    if (sock_wait_init(udp->base.rcv_wait) < 0) {
        log_error(LOG_UDP, "create rcv.wait failed");
        goto create_failed;
    }
    list_insert_last(&udp_list, &udp->base.node);
    return (sock_t*)udp;
create_failed:
    sock_uninit((sock_t *)udp);
    return (sock_t *)0;
}