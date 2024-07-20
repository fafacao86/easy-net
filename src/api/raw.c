#include "raw.h"
#include "easy_net_config.h"
#include "memory_pool.h"
#include "log.h"
#include "sock.h"
#include "socket.h"
#include "ipv4.h"

static raw_t raw_tbl[RAW_MAX_NR];
static memory_pool_t raw_mblock;
static list_t raw_list;


net_err_t raw_init(void) {
    log_info(LOG_RAW, "raw init.");

    memory_pool_init(&raw_mblock, raw_tbl, sizeof(raw_t), RAW_MAX_NR, LOCKER_NONE);
    init_list(&raw_list);

    log_info(LOG_RAW, "init done.");
    return NET_OK;
}



static net_err_t raw_sendto (struct _sock_t * sock, const void* buf, size_t len, int flags, const struct x_sockaddr* dest,
                             x_socklen_t dest_len, ssize_t * result_len) {
    // check if the socket is for this dest ip
    ipaddr_t dest_ip;
    struct x_sockaddr_in* addr = (struct x_sockaddr_in*)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    if (!ipaddr_is_any(&sock->remote_ip) && !ipaddr_is_equal(&dest_ip, &sock->remote_ip)) {
        log_error(LOG_RAW, "dest is incorrect");
        return NET_ERR_WRONG_SOCKET;
    }
    packet_t* pktbuf = packet_alloc((int)len);
    if (!pktbuf) {
        log_error(LOG_RAW, "no buffer");
        return NET_ERR_MEM;
    }
    net_err_t err = packet_write(pktbuf, (uint8_t *)buf, (int)len);
    if (sock->err < 0) {
        log_error(LOG_RAW, "copy data error");
        goto end_sendto;
    }
    err = ipv4_out(sock->protocol, &dest_ip, &netif_get_default()->ipaddr, pktbuf);
    if (err < 0) {
        log_error(LOG_RAW, "send error");
        goto end_sendto;
    }

    *result_len = (ssize_t)len;
    return NET_OK;
    end_sendto:
    packet_free(pktbuf);
    return err;
}


/**
 * create a raw socket.
 */
sock_t* raw_create(int family, int protocol) {
    static const sock_ops_t raw_ops = {
            .sendto = raw_sendto,
    };
    raw_t* raw = memory_pool_alloc(&raw_mblock, -1);
    if (!raw) {
        log_error(LOG_RAW, "no raw sock");
        return (sock_t*)0;
    }

    net_err_t err = sock_init((sock_t*)raw, family, protocol, &raw_ops);
    if (err < 0) {
        log_error(LOG_RAW, "create raw failed.");
        memory_pool_free(&raw_mblock, raw);
        return (sock_t*)0;
    }
    list_insert_last(&raw_list, &raw->base.node);
    return (sock_t *)raw;
}
