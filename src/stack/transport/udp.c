#include "udp.h"
#include "memory_pool.h"
#include "log.h"
#include "utils.h"
#include "ipv4.h"
#include "protocols.h"

static udp_t udp_tbl[UDP_MAX_NR];
static memory_pool_t udp_mblock;
static list_t udp_list;



#if LOG_DISP_ENABLED(LOG_UDP)
static void display_udp_list (void) {
    plat_printf("--- udp list\n --- ");

    int idx = 0;
    list_node_t * node;

    list_for_each(node, &udp_list) {
        udp_t * udp = (udp_t *)list_entry(node, sock_t, node);
        plat_printf("[%d]\n", idx++);
        dump_ip_buf("\tlocal:", (const uint8_t *)&udp->base.local_ip.a_addr);
        plat_printf("\tlocal port: %d\n", udp->base.local_port);
        dump_ip_buf("\tremote:", (const uint8_t *)&udp->base.remote_ip.a_addr);
        plat_printf("\tremote port: %d\n", udp->base.remote_port);
    }
}


static void display_udp_packet(udp_pkt_t * pkt) {
    plat_printf("UDP packet:\n");
    plat_printf("source Port:%d\n", pkt->hdr.src_port);
    plat_printf("dest Port: %d\n", pkt->hdr.dest_port);
    plat_printf("length: %d bytes\n", pkt->hdr.total_len);
    plat_printf("checksum:  %04x\n", pkt->hdr.checksum);
}
#else

#define display_udp_packet(packet)
static void display_udp_list (void)
#endif


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
 * API consumer may not specify the local port, we need to allocate one,
 * the local ip address may also be null, we need to lookup route table
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
    err = udp_out(&dest_ip, dport, &sock->local_ip, sock->local_port, pktbuf);
    if (err < 0) {
        log_error(LOG_UDP, "send error");
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



/**
 *  in this function, we mainly validate the local ip and port
 *  the ip has to be valid which means the ip has to be netif ip,
 *  and the ip-port pair has to be unused
 *  and one socket can only be bound once
 */
net_err_t udp_bind(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    const struct x_sockaddr_in* addr_in = (const struct x_sockaddr_in*)addr;
    // can not bind twice
    if (sock->local_port != NET_PORT_EMPTY) {
        log_error(LOG_UDP, "already binded.");
        return NET_ERR_BINED;
    }

    ipaddr_t local_ip;
    ipaddr_from_buf(&local_ip, (const uint8_t *)&addr_in->sin_addr.addr_array);
    int port = e_ntohs(addr_in->sin_port);

    // iterate the allocated sockets to check if there exists the same ip-port pair
    list_node_t* node;
    udp_t* udp = (udp_t*)0;
    list_for_each(node, &udp_list) {
        udp_t* u = (udp_t *)list_entry(node, sock_t, node);
        if ((sock_t*)u == sock) {
            continue;
        }
        if (ipaddr_is_equal(&sock->local_ip, &local_ip) && (sock->local_port == port)) {
            udp = u;
            break;
        }
    }
    if (udp) {
        log_error(LOG_UDP, "port already used!");
        return NET_ERR_BINED;
    } else {
        // bind the socket, just set the local ip and port
        sock_bind(sock, addr, len);
    }
    display_udp_list();
    return NET_OK;
}


net_err_t udp_recvfrom(sock_t* sock, void* buf, size_t len, int flags,
                       struct x_sockaddr* src, x_socklen_t* addr_len, ssize_t * result_len) {
    udp_t * udp = (udp_t *)sock;
    list_node_t * first = list_remove_first(&udp->recv_list);
    if (!first) {
        *result_len = 0;
        return NET_ERR_NEED_WAIT;
    }
    packet_t* pktbuf = list_entry(first, packet_t, node);
    assert_halt(pktbuf != (packet_t *)0, "pktbuf error");
    udp_from_t* from = (udp_from_t *)packet_data(pktbuf);
    struct x_sockaddr_in* addr = (struct x_sockaddr_in*)src;
    plat_memset(addr, 0, sizeof(struct x_sockaddr));
    addr->sin_family = AF_INET;
    addr->sin_port = e_htons(from->port);     // convert to network byte order
    ipaddr_to_buf(&from->from, addr->sin_addr.addr_array);
    packet_remove_header(pktbuf, sizeof(udp_from_t));
    int size = (pktbuf->total_size > (int)len) ? (int)len : pktbuf->total_size;
    packet_reset_pos(pktbuf);
    net_err_t err = packet_read(pktbuf, buf, size);
    if (err < 0) {
        packet_free(pktbuf);
        log_error(LOG_UDP, "pktbuf read error");
        return err;
    }
    packet_free(pktbuf);
    if (result_len) {
        *result_len = (ssize_t)size;
    }
    return NET_OK;
}


net_err_t udp_close(sock_t* sock) {
    display_udp_list();
    udp_t * udp = (udp_t *)sock;
    list_remove(&udp_list, &sock->node);
    list_node_t* node;
    while ((node = list_remove_first(&udp->recv_list))) {
        packet_t* buf = list_entry(node, packet_t, node);
        packet_free(buf);
    }
    memory_pool_free(&udp_mblock, sock);
    return NET_OK;
}

net_err_t udp_connect(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    sock_connect(sock, addr, len);
    display_udp_list();
    return NET_OK;
}

sock_t* udp_create(int family, int protocol) {
    static const sock_ops_t udp_ops = {
            .setopt = sock_setopt,
            .sendto = udp_sendto,
            .recvfrom = udp_recvfrom,
            .close = udp_close,
            .connect = udp_connect,
            .send = sock_send,
            .recv = sock_recv,
            .bind = udp_bind,
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


net_err_t udp_out(ipaddr_t * dest, uint16_t dport, ipaddr_t * src, uint16_t sport, packet_t * buf) {
    log_info(LOG_UDP, "send an udp packet!");

    // if the src is null, use the route table to find the src ip of interface
    if (!src || ipaddr_is_any(src)) {
        rentry_t* rt = rt_find(dest);
        if (rt == (rentry_t*)0) {
            dbg_dump_ip(LOG_UDP, "no route to dest: ", dest);
            return NET_ERR_UNREACH;
        }
        src = &rt->netif->ipaddr;
    }
    net_err_t err = packet_add_header(buf, sizeof(udp_hdr_t), 1);
    if (err < 0) {
        log_error(LOG_UDP, "add header failed. err = %d", err);
        return NET_ERR_SIZE;
    }

    udp_hdr_t * udp_hdr = (udp_hdr_t*)packet_data(buf);
    udp_hdr->src_port = sport;
    udp_hdr->dest_port = dport;
    udp_hdr->total_len = buf->total_size;
    udp_hdr->src_port = e_htons(udp_hdr->src_port);
    udp_hdr->dest_port = e_htons(udp_hdr->dest_port);
    udp_hdr->total_len = e_htons(udp_hdr->total_len);
    udp_hdr->checksum = checksum_peso(src->a_addr, dest->a_addr, NET_PROTOCOL_UDP, buf);
    err = ipv4_out(NET_PROTOCOL_UDP, dest, src, buf);
    if (err < 0) {
        log_error(LOG_UDP, "udp out error, err = %d", err);
        return err;
    }

    return err;
}

static sock_t* udp_find(ipaddr_t* src_ip, uint16_t sport, ipaddr_t* dest_ip, uint16_t dport) {
    // iterate allocated UDP sockets to find recipient
    // local ip can be null, which means multiple netif can match
    list_node_t* node;
    sock_t * found = (sock_t *)0;
    list_for_each(node, &udp_list) {
        sock_t* s = list_entry(node, sock_t, node);
        // dst port must match
        if (!dport || (s->local_port != dport)) {
            continue;
        }
        // check if local_ip is null or equal to dest_ip, null means listens on all netifs
        if (!ipaddr_is_any(&s->local_ip) && !ipaddr_is_equal(&s->local_ip, dest_ip)) {
            continue;
        }
        // check if remote_ip is null or equal to src_ip
        if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&s->remote_ip, src_ip)) {
            continue;
        }
        // check if remote_port is null or equal to sport
        if (s->remote_port && (s->remote_port != sport)) {
            continue;
        }
        found = s;
        break;
    }
    return found;
}

static net_err_t is_pkt_ok(udp_pkt_t * pkt, int size) {
    if ((size < sizeof(udp_hdr_t)) || (size < pkt->hdr.total_len)) {
        log_error(LOG_UDP, "udp packet size incorrect: %d!", size);
        return NET_ERR_SIZE;
    }
    return NET_OK;
}


net_err_t udp_in (packet_t* buf, ipaddr_t* src_ip, ipaddr_t* dest_ip) {
    log_info(LOG_UDP, "Recv a udp packet!");

    int iphdr_size = ipv4_hdr_size((ipv4_pkt_t *)packet_data(buf));
    net_err_t err = packet_set_cont(buf, sizeof(udp_hdr_t) + iphdr_size);
    if (err < 0) {
        log_error(LOG_UDP, "set udp cont failed");
        return err;
    }
    // skip the ip header
    ipv4_pkt_t * ip_pkt = (ipv4_pkt_t *)packet_data(buf);   //
    udp_pkt_t* udp_pkt = (udp_pkt_t*)((uint8_t *)ip_pkt + iphdr_size);
    uint16_t local_port = e_ntohs(udp_pkt->hdr.dest_port);
    uint16_t remote_port = e_ntohs(udp_pkt->hdr.src_port);

    // find recipient socket
    udp_t * udp = (udp_t*)udp_find(src_ip, remote_port, dest_ip, local_port);
    if (!udp) {
        log_error(LOG_UDP, "no udp for this packet");
        return NET_ERR_UNREACH;
    }
    // remove ip header
    packet_remove_header(buf, iphdr_size);
    udp_pkt = (udp_pkt_t*)packet_data(buf);
    if (udp_pkt->hdr.checksum) {
        packet_reset_pos(buf);
        if (checksum_peso(dest_ip->a_addr, src_ip->a_addr, NET_PROTOCOL_UDP, buf)) {
            log_warning(LOG_UDP, "udp check sum incorrect");
            return NET_ERR_BROKEN;
        }
    }
    udp_pkt = (udp_pkt_t*)(packet_data(buf));
    udp_pkt->hdr.src_port = e_ntohs(udp_pkt->hdr.src_port);
    udp_pkt->hdr.dest_port = e_ntohs(udp_pkt->hdr.dest_port);
    udp_pkt->hdr.total_len = e_ntohs(udp_pkt->hdr.total_len);
    if ((err = is_pkt_ok(udp_pkt, buf->total_size)) <  0) {
        log_error(LOG_UDP, "udp packet error");
        return err;
    }
    display_udp_packet(udp_pkt);

    packet_remove_header(buf, (int)(sizeof(udp_hdr_t) - sizeof(udp_from_t)));
    // this is to identify the datagram sender, because receiver might not know the remote ip
    // due to 0.0.0.0 binding
    udp_from_t* from = (udp_from_t *)packet_data(buf);
    from->port = remote_port;
    ipaddr_copy(&from->from, src_ip);

    if (list_count(&udp->recv_list) < UDP_MAX_RECV) {
        list_insert_last(&udp->recv_list, &buf->node);
        sock_wakeup((sock_t *)udp, SOCK_WAIT_READ, NET_OK);
    } else {
        log_warning(LOG_UDP, "queue full, drop pkt");
        packet_free(buf);
    }
    return NET_OK;
}
