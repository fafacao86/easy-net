#include "tcp.h"
#include "memory_pool.h"
#include "log.h"
#include "socket.h"
#include "utils.h"
#include "protocols.h"
#include "ipv4.h"


static tcp_t tcp_tbl[TCP_MAX_NR];
static memory_pool_t tcp_mblock;
static list_t tcp_list;



#if LOG_DISP_ENABLED(LOG_TCP)
void tcp_show_info (char * msg, tcp_t * tcp) {
    plat_printf("    local port: %u, remote port: %u\n", tcp->base.local_port, tcp->base.remote_port);
}

void tcp_display_pkt (char * msg, tcp_hdr_t * tcp_hdr, packet_t * buf) {
    plat_printf("%s\n", msg);
    plat_printf("    sport: %u, dport: %u\n", tcp_hdr->sport, tcp_hdr->dport);
    plat_printf("    seq: %u, ack: %u, win: %d\n", tcp_hdr->seq, tcp_hdr->ack, tcp_hdr->win);
    plat_printf("    flags:");
    if (tcp_hdr->f_syn) {
        plat_printf(" syn");
    }
    if (tcp_hdr->f_rst) {
        plat_printf(" rst");
    }
    if (tcp_hdr->f_ack) {
        plat_printf(" ack");
    }
    if (tcp_hdr->f_psh) {
        plat_printf(" push");
    }
    if (tcp_hdr->f_fin) {
        plat_printf(" fin");
    }

    plat_printf("\n    len=%d", buf->total_size - tcp_hdr_size(tcp_hdr));
    plat_printf("\n");
}

void tcp_show_list (void) {
    plat_printf("-------- tcp list -----\n");
    list_node_t * node;
    list_for_each(node, &tcp_list) {
        tcp_t * tcp = (tcp_t *)list_entry(node, sock_t, node);
        tcp_show_info("", tcp);
    }
}
#endif



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
    static const sock_ops_t tcp_ops = {
            .connect = tcp_connect,
            .close = tcp_close,
    };
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
    if (sock_wait_init(&tcp->conn.wait) < 0) {
        log_error(LOG_TCP, "create conn.wait failed");
        goto alloc_failed;
    }
    tcp->base.conn_wait = &tcp->conn.wait;
    return tcp;
alloc_failed:
    if (tcp->base.conn_wait) {
        sock_wait_destroy(tcp->base.conn_wait);
    }
    memory_pool_free(&tcp_mblock, tcp);
    return (tcp_t *)0;
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


net_err_t tcp_close(sock_t* sock) {
    return NET_OK;
}


/**
 * allocate a dynamic port for tcp connection
 */
int tcp_alloc_port(void) {
#if 1 // NET_DBG
    srand((unsigned int)time(NULL));
    int search_idx = rand() % 1000 + NET_PORT_DYN_START;
#else
    static int search_idx = NET_PORT_DYN_START;  // 搜索起点
#endif
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; i++) {
        list_node_t* node;
        list_for_each(node, &tcp_list) {
            sock_t* sock = list_entry(node, sock_t, node);
            if (sock->local_port == search_idx) {
                break;
            }
        }
        int port = search_idx++;
        if (search_idx >= NET_PORT_DYN_END) {
            search_idx = NET_PORT_DYN_START;
        }
        if (!node) {
            return port;
        }
    }
    return -1;
}

net_err_t tcp_connect(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    tcp_t * tcp = (tcp_t *)sock;
    // set remote ip and port
    const struct x_sockaddr_in* addr_in = (const struct x_sockaddr_in*)addr;
    ipaddr_from_buf(&sock->remote_ip, (uint8_t *)&addr_in->sin_addr.s_addr);
    sock->remote_port = e_ntohs(addr_in->sin_port);

    if (sock->local_port == NET_PORT_EMPTY) {
        int port = tcp_alloc_port();
        if (port == -1) {
            log_error(LOG_TCP, "alloc port failed.");
            return NET_ERR_NONE;
        }
        sock->local_port = port;
    }

    // set local ip by looking up route table
    if (ipaddr_is_any(&sock->local_ip)) {
        rentry_t * rt = rt_find(&sock->remote_ip);
        if (rt == (rentry_t*)0) {
            log_error(LOG_TCP, "no route to dest");
            return NET_ERR_UNREACH;
        }
        ipaddr_copy(&sock->local_ip, &rt->netif->ipaddr);
    }

    // wait for three-way handshake to complete
    return NET_ERR_NEED_WAIT;
}