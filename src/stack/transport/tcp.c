#include "tcp.h"
#include "memory_pool.h"
#include "log.h"
#include "socket.h"
#include "utils.h"
#include "protocols.h"
#include "ipv4.h"
#include "tcp_out.h"
#include "tcp_state.h"
#include "tcp_in.h"


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


static void tcp_keepalive_tmo(struct _net_timer_t* timer, void * arg) {
    tcp_t * tcp = (tcp_t *)arg;
    if (++tcp->conn.keep_retry <= tcp->conn.keep_cnt) {
        tcp_send_keepalive(tcp);
        net_timer_remove(&tcp->conn.keep_timer);
        net_timer_add(&tcp->conn.keep_timer, "keepalive", tcp_keepalive_tmo, tcp, tcp->conn.keep_intvl*1000, 0);
        log_info(LOG_TCP, "tcp keepalive tmo, retrying: %d", tcp->conn.keep_retry);
    } else {
        tcp_send_reset_for_tcp(tcp);
        tcp_abort(tcp, NET_ERR_TIMEOUT);
        log_error(LOG_TCP, "tcp keepalive tmo, give up");
    }
}


static void keepalive_start_timer (tcp_t * tcp) {
    tcp->conn.keep_retry = 0;
    net_timer_add(&tcp->conn.keep_timer, "keepalive", tcp_keepalive_tmo, tcp, tcp->conn.keep_idle*1000, 0);
    log_info(LOG_TCP, "tcp keepalive enabled.");
}

void tcp_keepalive_start (tcp_t * tcp, int run) {
    if (tcp->flags.keep_enable && !run) {
        net_timer_remove(&tcp->conn.keep_timer);
        log_info(LOG_TCP, "keepalive disabled");
    } else if (!tcp->flags.keep_enable && run) {
        keepalive_start_timer(tcp);
    }
    tcp->flags.keep_enable = run;
}


void tcp_keepalive_restart (tcp_t * tcp) {
    if (tcp->flags.keep_enable) {
        net_timer_remove(&tcp->conn.keep_timer);
        keepalive_start_timer(tcp);
        tcp->conn.keep_retry = 0;
    }
}



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

net_err_t tcp_setopt(struct _sock_t* sock,  int level, int optname, const char * optval, int optlen) {
    // more general options are handled by sock_setopt
    net_err_t err = sock_setopt(sock, level, optname, optval, optlen);
    if (err == NET_OK) {
        return NET_OK;
    } else if ((err < 0) && (err != NET_ERR_NOT_SUPPORT)) {
        return err;
    }
    tcp_t * tcp = (tcp_t *)sock;
    if (level == SOL_SOCKET) {
        if (optlen != sizeof(int)) {
            log_error(LOG_TCP, "param size error");
            return NET_ERR_PARAM;
        }
        tcp->flags.keep_enable = *(int *)optval;
        tcp_keepalive_start(tcp, *(int *)optval);
    } else if (level == SOL_TCP) {
        switch (optname) {
            case TCP_KEEPIDLE:
                if (optlen != sizeof(int)) {
                    log_error(LOG_TCP, "param size error");
                    return NET_ERR_PARAM;
                }
                tcp->conn.keep_idle = *(int *)optval;
                tcp_keepalive_restart(tcp);
                return NET_OK;
            case TCP_KEEPINTVL:
                if (optlen != sizeof(int)) {
                    log_error(LOG_TCP, "param size error");
                    return NET_ERR_PARAM;
                }
                tcp->conn.keep_intvl = *(int *)optval;
                tcp_keepalive_restart(tcp);
                return NET_OK;
            case TCP_KEEPCNT:
                if (optlen != sizeof(int)) {
                    log_error(LOG_TCP, "param size error");
                    return NET_ERR_PARAM;
                }
                tcp->conn.keep_cnt = *(int *)optval;
                tcp_keepalive_restart(tcp);
                return NET_OK;
            default:
                log_error(LOG_TCP, "unknown param");
                break;
        }
    }
    return NET_ERR_PARAM;
}

/**
 * bind a local port to listen
 */
net_err_t tcp_bind(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    tcp_t * tcp = (tcp_t *)sock;
    // check state
    if (tcp->state != TCP_STATE_CLOSED) {
        log_error(LOG_TCP, "tcp is not closed. connect is not allowed");
        return NET_ERR_STATE;
    }
    if (sock->local_port != NET_PORT_EMPTY) {
        log_error(LOG_UDP, "already binded.");
        return NET_ERR_PARAM;
    }
    const struct x_sockaddr_in* addr_in = (const struct x_sockaddr_in*)addr;
    if (addr_in->sin_port == NET_PORT_EMPTY) {
        log_error(LOG_TCP, "port is emptry");
        return NET_ERR_PARAM;
    }
    ipaddr_t local_ip;
    ipaddr_from_buf(&local_ip, (const uint8_t*)&addr_in->sin_addr);
    if (!ipaddr_is_any(&local_ip)) {
        // check if the ipaddr is valid, if there
        rentry_t* rt = rt_find(&local_ip);
        if (rt == (rentry_t*)0) {
            log_error(LOG_TCP, "ipaddr error, no netif has this ip");
            return NET_ERR_ADDR;
        }
        if (!ipaddr_is_equal(&local_ip, &rt->netif->ipaddr)) {
            log_error(LOG_TCP, "ipaddr error");
            return NET_ERR_ADDR;
        }
    }
    // check if the port is binded
    list_node_t * node;
    list_for_each(node, &tcp_list) {
        sock_t * curr = (sock_t *)list_entry(node, sock_t, node);
        // if the remote_port is not empty, it means the socket is for connection
        // not for listening, so we should skip it.
        if ((sock == curr) || (curr->remote_port != NET_PORT_EMPTY)) {
            continue;
        }
//        log_info(LOG_TCP, "port in use %d and try to bind port %d", curr->local_port, addr_in->sin_port);
        if (ipaddr_is_equal(&curr->local_ip, &local_ip) && (curr->local_port == e_ntohs(addr_in->sin_port))) {
            log_error(LOG_TCP, "ipaddr and port already used");
            return NET_ERR_ADDR;
        }
    }
    ipaddr_copy(&sock->local_ip, &local_ip);
    sock->local_port = e_ntohs(addr_in->sin_port);
    return NET_OK;
}



static tcp_t* tcp_alloc(int wait, int family, int protocol) {
    static const sock_ops_t tcp_ops = {
            .connect = tcp_connect,
            .close = tcp_close,
            .send = tcp_send,
            .recv = tcp_recv,
            .setopt = tcp_setopt,
            .bind = tcp_bind,
            .listen = tcp_listen,
            .accept = tcp_accept,
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
    tcp->flags.keep_enable = 0;
    tcp->conn.keep_idle = TCP_KEEPALIVE_TIME;
    tcp->conn.keep_intvl = TCP_KEEPALIVE_INTVL;
    tcp->conn.keep_cnt = TCP_KEEPALIVE_PROBES;
    tcp->state = TCP_STATE_CLOSED;
    // sender and receiver window variables
    tcp->snd.una = tcp->snd.nxt = tcp->snd.iss = 0;
    tcp->rcv.nxt = tcp->rcv.iss = 0;
    if (sock_wait_init(&tcp->snd.wait) < 0) {
        log_error(LOG_TCP, "create snd.wait failed");
        goto alloc_failed;
    }
    tcp->base.snd_wait = &tcp->snd.wait;
    if (sock_wait_init(&tcp->rcv.wait) < 0) {
        log_error(LOG_TCP, "create rcv.wait failed");
        goto alloc_failed;
    }
    tcp->base.rcv_wait = &tcp->rcv.wait;
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


/**
 * in RFC 793, the iss is generated using clock
 * here for simplicity, we just use a static random sequence number
 * */
static uint32_t tcp_get_iss(void) {
    static uint32_t seq = 0;
#if 0
    seq += seq == 0 ? clock() : 305;
#else
    seq += seq == 0 ? 32435 : 305;
#endif
    return seq;
}


/**
 * before connect we need to initialize the window variables
 * and generate a new initial sequence number (iss)
 * */
static net_err_t tcp_init_connect(tcp_t * tcp) {
    tcp_buf_init(&tcp->snd.buf, tcp->snd.data, TCP_SBUF_SIZE);
    tcp->snd.iss = tcp_get_iss();
    tcp->snd.una = tcp->snd.nxt = tcp->snd.iss;
    tcp_buf_init(&tcp->rcv.buf, tcp->rcv.data, TCP_RBUF_SIZE);
    tcp->rcv.nxt = 0;
    tcp->mss = TCP_MSS;
    return NET_OK;
}


net_err_t tcp_connect(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    tcp_t * tcp = (tcp_t *)sock;
    if (tcp->state != TCP_STATE_CLOSED) {
        log_error(LOG_TCP, "tcp is not closed. connect is not allowed");
        return NET_ERR_STATE;
    }
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
    net_err_t err;
    // initialize window variables, before sending SYN
    if ((err = tcp_init_connect(tcp)) < 0) {
        log_error(LOG_TCP, "init conn failed.");
        return err;
    }
    if ((err = tcp_send_syn(tcp)) < 0) {
        log_error(LOG_TCP, "send syn failed.");
        return err;
    }
    tcp_set_state(tcp, TCP_STATE_SYN_SENT);
    // wait for three-way handshake to complete
    return NET_ERR_NEED_WAIT;
}


/**
 * if not perfectly matched, search for listen socket
 * */
sock_t* tcp_find(ipaddr_t * local_ip, uint16_t local_port, ipaddr_t * remote_ip, uint16_t remote_port) {
    sock_t* match = (sock_t*)0;
    list_node_t* node;
    list_for_each(node, &tcp_list) {
        sock_t* s = list_entry(node, sock_t, node);

        // 4-tuple perfectly matched
        if (ipaddr_is_equal(&s->local_ip, local_ip) && (s->local_port == local_port) &&
            ipaddr_is_equal(&s->remote_ip, remote_ip) && (s->remote_port == remote_port)) {
            return s;
        }
        // for listen socket, if exactly matched, return it, else check for wildcard
        tcp_t * tcp = (tcp_t *)s;
        if ((tcp->state == TCP_STATE_LISTEN) && (s->local_port == local_port)) {
            if (ipaddr_is_equal(&s->local_ip, local_ip)) {
                return s;
            } else if (ipaddr_is_any(&s->local_ip)) {
                match = s;
            }
        }
    }
    return (sock_t*)match;
}


/**
 * abort tcp connection, enter CLOSED state,
 * notify application if there is any threads waiting on this socket
 * the release of resources should be done by the application
 */
net_err_t tcp_abort (tcp_t * tcp, int err) {
    tcp_set_state(tcp, TCP_STATE_CLOSED);
    sock_wakeup(&tcp->base, SOCK_WAIT_ALL, err);
    return NET_OK;
}



void tcp_free(tcp_t* tcp) {
    assert_halt(tcp->state != TCP_STATE_FREE, "tcp free");
    sock_wait_destroy(&tcp->conn.wait);
    sock_wait_destroy(&tcp->snd.wait);
    sock_wait_destroy(&tcp->rcv.wait);
    tcp->state = TCP_STATE_FREE;        // this is for debug purpose
    list_remove(&tcp_list, &tcp->base.node);
    memory_pool_free(&tcp_mblock, tcp);
}



net_err_t tcp_close(sock_t* sock) {
    tcp_t* tcp = (tcp_t*)sock;
    log_info(LOG_TCP, "closing tcp: state = %s", tcp_state_name(tcp->state));

    // if already closed, return
    switch (tcp->state) {
        case TCP_STATE_CLOSED:
            log_info(LOG_TCP, "tcp already closed");
            tcp_free(tcp);
            return NET_OK;
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_SYN_SENT:        // if connection has not been established, abort and free tcb
            tcp_abort(tcp, NET_ERR_CLOSED);
            tcp_free(tcp);
            return NET_OK;
        case TCP_STATE_CLOSE_WAIT:
            // passive close, send fin, enter last_ack and wait for ack
            tcp_send_fin(tcp);
            tcp_set_state(tcp, TCP_STATE_LAST_ACK);
            return NET_ERR_NEED_WAIT;
        case TCP_STATE_ESTABLISHED:
            // active close, send fin, enter fin_wait_1 and wait for ack
            tcp_send_fin(tcp);
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_1);
            return NET_ERR_NEED_WAIT;
        default:
            // other states, return error
            log_error(LOG_TCP, "tcp state error[%s]: send is not allowed", tcp_state_name(tcp->state));
            return NET_ERR_STATE;
    }
}


/**
 * only allowed in ESTABLISHED or CLOSE_WAIT state
 */
net_err_t tcp_send (struct _sock_t* sock, const void* buf, size_t len, int flags, ssize_t * result_len) {
    tcp_t* tcp = (tcp_t*)sock;

    switch (tcp->state) {
        case TCP_STATE_CLOSED:
            log_error(LOG_TCP, "tcp closed: send is not allowed");
            return NET_ERR_CLOSED;
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_CLOSING:
        case TCP_STATE_TIME_WAIT:
        case TCP_STATE_LAST_ACK:
            // active close, do not allow to send data anymore
            log_error(LOG_TCP, "tcp closed[%s]: send is not allowed", tcp_state_name(tcp->state));
            return NET_ERR_CLOSED;
        case TCP_STATE_ESTABLISHED:
        case TCP_STATE_CLOSE_WAIT: {
            // established or passive close, send data
            break;
        }
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_SYN_SENT:
        default:
            // do not allow data piggybacking in SYN, though it is allowed in RFC 793
            // BSD sockets do not support data piggybacking in SYN neither
            log_error(LOG_TCP, "tcp state error[%s]: send is not allowed", tcp_state_name(tcp->state));
            return NET_ERR_STATE;
    }
    ssize_t size = tcp_write_sndbuf(tcp, (uint8_t *)buf, (int)len);
    if (size <= 0) {
        // when buffer is full, wait for data to be sent and acked
        *result_len = 0;
        return NET_ERR_NEED_WAIT;
    } else {
        *result_len = size;
        tcp_transmit(tcp);
        return NET_OK;
    }
}


net_err_t tcp_recv (struct _sock_t* s, void* buf, size_t len, int flags, ssize_t * result_len) {
    tcp_t* tcp = (tcp_t*)s;
    net_err_t need_wait = NET_ERR_NEED_WAIT;
    switch (tcp->state) {
        case TCP_STATE_LAST_ACK:
        case TCP_STATE_CLOSED:
            log_error(LOG_TCP, "tcp closed");
            return NET_ERR_CLOSED;
        case TCP_STATE_CLOSE_WAIT:
        case TCP_STATE_CLOSING:
            // the passive close, we can still call recv() but no need to wait
            need_wait = NET_OK;
            break;
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_ESTABLISHED:
            break;
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_TIME_WAIT:
        default:
            log_error(LOG_TCP, "tcp state error");
            return NET_ERR_STATE;
    }
    *result_len = 0;
    int cnt = tcp_buf_read_rcv(&tcp->rcv.buf, buf, (int)len);
    if (cnt > 0) {
        *result_len = cnt;
        return NET_OK;
    }
    return need_wait;
}



net_err_t tcp_listen (struct _sock_t* s, int backlog) {
    tcp_t * tcp = (tcp_t *)s;
    if (backlog <= 0) {
        log_error(LOG_TCP, "backlog(%d) <= 0", backlog);
        return NET_ERR_PARAM;
    }
    if (tcp->state != TCP_STATE_CLOSED) {
        log_error(LOG_TCP, "tcp is not closed. listen is not allowed");
        return NET_ERR_STATE;
    }
    tcp->state = TCP_STATE_LISTEN;
    tcp->conn.backlog = backlog;
    return NET_OK;
}



net_err_t tcp_accept (struct _sock_t *s, struct x_sockaddr* addr, x_socklen_t* len, struct _sock_t ** client) {
    list_node_t * node;
    list_for_each(node, &tcp_list) {
        sock_t * sock = list_entry(node, sock_t, node);
        tcp_t * tcp = (tcp_t *)sock;

        if ((sock == s) || (tcp->parent != (tcp_t *)s)) {
            continue;
        }
        if (tcp->flags.inactive) {
            struct x_sockaddr_in * addr_in = (struct x_sockaddr_in *)addr;
            plat_memset(addr_in, 0, sizeof(struct x_sockaddr_in));
            addr_in->sin_family = AF_INET;
            addr_in->sin_port = e_htons(tcp->base.remote_port);
            ipaddr_to_buf(&tcp->base.remote_ip, (uint8_t *)&addr_in->sin_addr.s_addr);
            if (len) {
                *len = sizeof(struct x_sockaddr_in);
            }
            tcp->flags.inactive = 0;
            *client = (sock_t *)tcp;
            return NET_OK;
        }
    }
    return NET_ERR_NEED_WAIT;
}


/**
 * get number of inactive tcp child sockets of a parent tcp socket
 */
int tcp_backlog_count (tcp_t * tcp) {
    int count = 0;
    list_node_t * node;
    list_for_each(node, &tcp_list) {
        sock_t * sock = list_entry(node, sock_t, node);
        tcp_t * child = (tcp_t *)sock;
        if ((child->parent == tcp) && (child->flags.inactive)) {
            count++;
        }
    }
    return count;
}



/**
 * spawn a child tcp socket from a parent tcp socket
 * and incoming SYN segment to extract window variables
 */
tcp_t * tcp_create_child (tcp_t * parent, tcp_seg_t * seg) {
    tcp_t * child = (tcp_t *)tcp_alloc(0, parent->base.family, parent->base.protocol);
    if (!child) {
        log_error(LOG_TCP, "no child tcp");
        return (tcp_t *)0;
    }
    ipaddr_copy(&child->base.local_ip, &seg->local_ip);
    ipaddr_copy(&child->base.remote_ip, &seg->remote_ip);
    child->base.local_port = seg->hdr->dport;
    child->base.remote_port = seg->hdr->sport;
    child->parent = parent;
    child->flags.irs_valid = 1;
    child->flags.inactive = 1;                  // not accepted yet
    child->conn.backlog = 0;

    tcp_init_connect(child);                    // init send window variables
    child->rcv.iss = seg->seq;
    child->rcv.nxt = child->rcv.iss + 1;        // skip the SYN logic byte

    // this is mainly for MSS negotiation
    tcp_read_options(child, seg->hdr);

    tcp_insert(child);
    return child;
}