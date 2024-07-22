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
    err = ipv4_out(sock->protocol, &dest_ip, &sock->local_ip, pktbuf);
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
 * If the buffer is empty, return NET_ERR_NEED_WAIT, let the consumer to wait for data.
 * Copy the data to the user buffer,
 * return the actual length of data received and src address via pointers provided by consumer.
 * */
static net_err_t raw_recvfrom (struct _sock_t* sock, void* buf, size_t len, int flags,
                               struct x_sockaddr* src, x_socklen_t * addr_len, ssize_t * result_len) {
    log_info(LOG_RAW, "raw recvfrom\n");
    raw_t * raw = (raw_t *)sock;
//    *result_len = 0;
//    return NET_ERR_NEED_WAIT;
    list_node_t * first = list_remove_first(&raw->recv_list);
    if (!first) {
        *result_len = 0;
        return NET_ERR_NEED_WAIT;
    }

    packet_t* pPacket = list_entry(first, packet_t, node);
    assert_halt(pPacket != (packet_t *)0, "pktbuf error");
    // fill the source address, return to caller
    ipv4_hdr_t* iphdr = (ipv4_hdr_t*)packet_data(pPacket);
    struct x_sockaddr_in* addr = (struct x_sockaddr_in*)src;
    plat_memset(addr, 0, sizeof(struct x_sockaddr));
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    plat_memcpy(&addr->sin_addr, iphdr->src_ip, IPV4_ADDR_SIZE);

    int size = (pPacket->total_size > (int)len) ? (int)len : pPacket->total_size;
    packet_reset_pos(pPacket);
    // copy data to user buffer
    net_err_t err= packet_read(pPacket, buf, size);
    if (err < 0) {
        packet_free(pPacket);
        log_error(LOG_RAW, "pktbuf read error");
        return err;
    }
    packet_free(pPacket);
    // fill the actual length of data received
    *result_len = size;
    return NET_OK;
}

net_err_t raw_close(sock_t * sock) {
    raw_t * raw = (raw_t *)sock;
    list_remove(&raw_list, &sock->node);
    list_node_t* node;
    while ((node = list_remove_first(&raw->recv_list))) {
        packet_t* buf = list_entry(node, packet_t, node);
        packet_free(buf);
    }
    sock_uninit(sock);
    memory_pool_free(&raw_mblock, sock);
    return NET_OK;
}


/**
 * create a raw socket.
 */
sock_t* raw_create(int family, int protocol) {
    static const sock_ops_t raw_ops = {
            .sendto = raw_sendto,
            .recvfrom = raw_recvfrom,
            .setopt = sock_setopt,
            .close = raw_close,
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
    init_list(&raw->recv_list);
    raw->base.rcv_wait = &raw->rcv_wait;
    if (sock_wait_init(raw->base.rcv_wait) < 0) {
        log_error(LOG_RAW, "create rcv.wait failed");
        goto create_failed;
    }
    list_insert_last(&raw_list, &raw->base.node);
    return (sock_t *)raw;
create_failed:
    sock_uninit((sock_t *)raw);
    return (sock_t *)0;
}

/**
 * Tuple src-dst-protocol can pinpoint a raw socket.
 * */
static raw_t * raw_find (ipaddr_t * src, ipaddr_t * dest, int protocol) {
    list_node_t* node;
    raw_t * found = (raw_t *)0;
    list_for_each(node, &raw_list) {
        raw_t* raw = (raw_t *)list_entry(node, sock_t, node);
        if (raw->base.protocol && (raw->base.protocol != protocol)) {
            continue;
        }
        if (!ipaddr_is_any(&raw->base.local_ip) && !ipaddr_is_equal(&raw->base.local_ip, dest)) {
            continue;
        }
        if (!ipaddr_is_any(&raw->base.remote_ip) && !ipaddr_is_equal(&raw->base.remote_ip, src)) {
            continue;
        }
        found = raw;
        break;
    }
    return found;
}

/**
 * this will be called by ipv4_in when it receives ip packets.
 * pass ip packet to raw socket. wake up previously waiting recvfrom
 * */
net_err_t raw_in(packet_t* packet) {
    ipv4_hdr_t* iphdr = (ipv4_hdr_t*)packet_data(packet);
    net_err_t err = NET_ERR_UNREACH;

    ipaddr_t src, dest;
    ipaddr_from_buf(&dest, iphdr->dest_ip);
    ipaddr_from_buf(&src, iphdr->src_ip);

    // to the corresponding raw socket
    raw_t * raw = raw_find(&src, &dest, iphdr->protocol);
    if (raw == (raw_t *)0) {
        log_warning(LOG_RAW, "no raw for this packet");
        return NET_ERR_UNREACH;
    }
    if (list_count(&raw->recv_list) < RAW_MAX_RECV) {
        list_insert_last(&raw->recv_list, &packet->node);
        sock_wakeup((sock_t *)raw, SOCK_WAIT_READ, NET_OK);
    } else {
        // if the buffer queue is full, drop the packet
        packet_free(packet);
    }
    return NET_OK;
}
