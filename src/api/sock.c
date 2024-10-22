#include "sys_plat.h"
#include "msg_handler.h"
#include "log.h"
#include "raw.h"
#include "socket.h"
#include "sock.h"
#include "udp.h"
#include "utils.h"
#include "ipv4.h"
#include "tcp.h"

#define SOCKET_MAX_NR		10
static x_socket_t socket_tbl[SOCKET_MAX_NR];

static inline int get_index(x_socket_t* socket) {
    return (int)(socket - socket_tbl);
}

static inline x_socket_t* get_socket(int idx) {
    if ((idx < 0) || (idx >= SOCKET_MAX_NR)) {
        return (x_socket_t*)0;
    }

    x_socket_t* s = socket_tbl + idx;
    return s;
}

static x_socket_t * socket_alloc (void) {
    x_socket_t * s = (x_socket_t *)0;
    // scan from 0 to SOCKET_MAX_NR to find a free socket
    // like file descriptor allocation in Linux
    for (int i = 0; i < SOCKET_MAX_NR; i++) {
        x_socket_t * curr = socket_tbl + i;
        if (curr->state == SOCKET_STATE_FREE) {
            s = curr;
            s->state = SOCKET_STATE_USED;
            break;
        }
    }
    return s;
}


static void socket_free(x_socket_t* s) {
    s->state = SOCKET_STATE_FREE;
}

net_err_t socket_init(void) {
    plat_memset(socket_tbl, 0, sizeof(socket_tbl));
    raw_init();
    return NET_OK;
}



net_err_t sock_create_req_in(func_msg_t* api_msg) {
    // sock for different protocols
    static const struct sock_info_t {
        int protocol;
        sock_t* (*create) (int family, int protocol);
    }  sock_tbl[] = {
            [SOCK_RAW] = { .protocol = 0, .create = raw_create,},
            [SOCK_DGRAM] = { .protocol = IPPROTO_UDP, .create = udp_create,},
            [SOCK_STREAM] = {.protocol = IPPROTO_TCP,  .create = tcp_create,},
    };
    sock_req_t * req = (sock_req_t *)api_msg->param;
    sock_create_t * param = &req->create;

    x_socket_t * socket = socket_alloc();
    if (socket == (x_socket_t *)0) {
        log_error(LOG_SOCKET, "no socket");
        return NET_ERR_MEM;
    }
    if ((param->type < 0) || (param->type >= sizeof(sock_tbl) / sizeof(sock_tbl[0]))) {
        log_error(LOG_SOCKET, "unknown type: %d", param->type);
        socket_free(socket);
        return NET_ERR_PARAM;
    }

    // create sock according to the type
    const struct sock_info_t* info = sock_tbl + param->type;
    if (param->protocol == 0) {
        param->protocol = info->protocol;
    }
    sock_t * sock = info->create(param->family, param->protocol);
    if (!sock) {
        log_error(LOG_SOCKET, "create sock failed, type: %d", param->type);
        socket_free(socket);
        return NET_ERR_MEM;
    }

    socket->sock = sock;
    req->sockfd = get_index(socket);
    return NET_OK;
}

net_err_t sock_init(sock_t* sock, int family, int protocol, const sock_ops_t * ops) {
    sock->protocol = protocol;
    sock->ops = ops;
    sock->family = family;
    ipaddr_set_any(&sock->local_ip);
    ipaddr_set_any(&sock->remote_ip);
    sock->local_port = 0;
    sock->remote_port = 0;
    sock->err = NET_OK;
    sock->rcv_tmo = 0;
    sock->snd_tmo = 0;
    list_node_init(&sock->node);
    sock->conn_wait = (sock_wait_t *)0;
    sock->snd_wait = (sock_wait_t *)0;
    sock->rcv_wait = (sock_wait_t *)0;
    return NET_OK;
}


void sock_uninit (sock_t * sock){
    if (sock->snd_wait) {
        sock_wait_destroy(sock->snd_wait);
    }
    if (sock->rcv_wait) {
        sock_wait_destroy(sock->rcv_wait);
    }
    if (sock->conn_wait) {
        sock_wait_destroy(sock->conn_wait);
    }
}



net_err_t sock_sendto_req_in (func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_data_t * data = (sock_data_t *)&req->data;
    if (!sock->ops->sendto) {
        log_error(LOG_SOCKET, "this function is not implemented");
        return NET_ERR_NOT_SUPPORT;
    }
    net_err_t err = sock->ops->sendto(sock, data->buf, data->len, data->flags,
                                      data->addr, *data->addr_len, &req->data.comp_len);
    if (err == NET_ERR_NEED_WAIT) {
        if (sock->snd_wait) {
            sock_wait_add(sock->snd_wait, sock->snd_tmo, req);
        }
    }
    return err;
}


net_err_t sock_recvfrom_req_in(func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_data_t * data = (sock_data_t *)&req->data;
    if (!sock->ops->recvfrom) {
        log_error(LOG_SOCKET, "this function is not implemented");
        return NET_ERR_NOT_SUPPORT;
    }
    net_err_t err = sock->ops->recvfrom(sock, data->buf, data->len, data->flags,
                                        data->addr, data->addr_len, &req->data.comp_len);
    if (err == NET_ERR_NEED_WAIT) {
        if (sock->rcv_wait) {
            log_info(LOG_SOCKET, "sock %p add to rcv wait %p", sock, sock->rcv_wait);
            sock_wait_add(sock->rcv_wait, sock->rcv_tmo, req);
        }
    }
    return err;
}



net_err_t sock_wait_init (sock_wait_t * wait) {
    wait->waiting = 0;
    wait->sem = sys_sem_create(0);
    return wait->sem == SYS_SEM_INVALID ? NET_ERR_SYS : NET_OK;
}

void sock_wait_destroy (sock_wait_t * wait) {
    if (wait->sem != SYS_SEM_INVALID) {
        sys_sem_free(wait->sem);
    }
}

/**
 * this is will be actively called by api consumer thread,
 * after it received a response with a wait in it
 */
net_err_t sock_wait_enter (sock_wait_t * wait, int tmo) {
    if (sys_sem_wait(wait->sem, tmo) < 0) {
        return NET_ERR_TIMEOUT;
    }
    return wait->err;
}

/**
 * called by handler thread, if it wants the api consumer to wait for a response
 */
void sock_wait_add (sock_wait_t * wait, int tmo, struct _sock_req_t * req) {
    req->wait = wait;
    req->wait_tmo = tmo;
    wait->waiting++;
}

/**
 * called by handler thread, if it wants the api consumer to wake up
 */
void sock_wait_leave (sock_wait_t * wait, net_err_t err) {
    log_info(LOG_SOCKET, "sock_wait_leave: %d", wait->waiting);
    if (wait->waiting > 0) {
        wait->waiting--;
        wait->err = err;
        sys_sem_notify(wait->sem);
        log_info(LOG_SOCKET, "notify sock_wait_leave: %d", wait->waiting);
    }
}


net_err_t sock_setsockopt_req_in(func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_opt_t * opt = (sock_opt_t *)&req->opt;
    return sock->ops->setopt(sock, opt->level, opt->optname, opt->optval, opt->optlen);
}


net_err_t sock_setopt(struct _sock_t* sock,  int level, int optname, const char * optval, int optlen) {
    // socket options only support SOL_SOCKET level
    if (level != SOL_SOCKET) {
        //log_error(LOG_SOCKET, "unknow level: %d", level);
        return NET_ERR_NOT_SUPPORT;
    }

    switch (optname) {
        case SO_RCVTIMEO:
        case SO_SNDTIMEO: {
            if (optlen != sizeof(struct x_timeval)) {
                log_error(LOG_SOCKET, "time size error");
                return NET_ERR_PARAM;
            }
            struct x_timeval * time = (struct x_timeval *)optval;
            int time_ms = time->tv_sec * 1000 + time->tv_usec / 1000;
            if (optname == SO_RCVTIMEO) {
                sock->rcv_tmo = time_ms;
                return NET_OK;
            } else if (optname == SO_SNDTIMEO) {
                sock->snd_tmo = time_ms;
                return NET_OK;
            } else {
                return NET_ERR_PARAM;
            }
        }
        default:
            break;
    }
    return NET_ERR_NOT_SUPPORT;
}


void sock_wakeup (sock_t * sock, int type, int err) {
    if (type & SOCK_WAIT_CONN) {
        sock_wait_leave(sock->conn_wait, err);
    }

    if (type & SOCK_WAIT_WRITE) {
        sock_wait_leave(sock->snd_wait, err);
    }

    if (type & SOCK_WAIT_READ) {
        sock_wait_leave(sock->rcv_wait, err);
    }
    sock->err = err;
}


net_err_t sock_close_req_in (func_msg_t* api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    net_err_t err = sock->ops->close(sock);
    // there might be tcp close handshake in progress, wait on the socket
    if (err == NET_ERR_NEED_WAIT) {
        sock_wait_add(sock->conn_wait, sock->rcv_tmo, req);
        return err;
    }
    return err;
}

net_err_t sock_connect_req_in (func_msg_t* api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_conn_t * conn = &req->conn;

    net_err_t err = sock->ops->connect(sock, conn->addr, conn->len);
    if (err == NET_ERR_NEED_WAIT) {
        if (sock->conn_wait) {
            sock_wait_add(sock->conn_wait, sock->rcv_tmo, req);
        }
    }
    return err;
}


net_err_t sock_connect(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    struct x_sockaddr_in* remote = (struct x_sockaddr_in*)addr;
    ipaddr_from_buf(&sock->remote_ip, remote->sin_addr.addr_array);
    sock->remote_port = e_ntohs(remote->sin_port);
    return NET_OK;
}



net_err_t sock_send_req_in (func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_data_t * data = (sock_data_t *)&req->data;

    sock->err = NET_OK;
    net_err_t err = sock->ops->send(sock, data->buf, data->len, data->flags, &req->data.comp_len);
    if (err == NET_ERR_NEED_WAIT) {
        if (sock->snd_wait) {
            sock_wait_add(sock->snd_wait, sock->snd_tmo, req);
        }
    }
    return err;
}

/**
 * wrapper function for sendto, taken the remote ip bound on the socket
 * */
net_err_t sock_send (struct _sock_t * sock, const void* buf, size_t len, int flags, ssize_t * result_len) {
    if (ipaddr_is_any(&sock->remote_ip)) {
        log_error(LOG_RAW, "dest ip is empty.");
        return NET_ERR_UNREACH;
    }

    struct x_sockaddr_in dest;
    dest.sin_family = sock->family;
    dest.sin_port = e_htons(sock->remote_port);
    ipaddr_to_buf(&sock->remote_ip, (uint8_t*)&dest.sin_addr);

    // get the remote ip and port then call sendto
    return sock->ops->sendto(sock, buf, len, flags, (const struct x_sockaddr *)&dest, sizeof(dest), result_len);
}



net_err_t sock_recv_req_in(func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_data_t * data = (sock_data_t *)&req->data;

    net_err_t err = sock->ops->recv(sock, data->buf, data->len, data->flags, &req->data.comp_len);
    if (err == NET_ERR_NEED_WAIT) {
        if (sock->rcv_wait) {
            sock_wait_add(sock->rcv_wait, sock->rcv_tmo, req);
        }
    }

    return err;
}


net_err_t sock_recv (struct _sock_t * sock, void* buf, size_t len, int flags, ssize_t * result_len) {
    if (ipaddr_is_any(&sock->remote_ip)) {
        log_error(LOG_RAW, "src ip is empty.socket is not connected");
        return NET_ERR_PARAM;
    }
    struct x_sockaddr src;
    x_socklen_t addr_len;
    return sock->ops->recvfrom(sock, buf, len, flags, &src, &addr_len, result_len);
}


net_err_t sock_bind_req_in(func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_bind_t * bind = (sock_bind_t *)&req->bind;
    return sock->ops->bind(sock, bind->addr, bind->len);
}



net_err_t sock_bind(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len) {
    ipaddr_t local_ip;
    struct x_sockaddr_in* local = (struct x_sockaddr_in*)addr;
    ipaddr_from_buf(&local_ip, local->sin_addr.addr_array);

    if (!ipaddr_is_any(&local_ip)) {
        // validate if the ipaddr is one of the netif ipaddr by looking up the route table
        rentry_t * rt = rt_find(&local_ip);
        if (!ipaddr_is_equal(&rt->netif->ipaddr, &local_ip)) {
            log_error(LOG_SOCKET, "addr error");
            return NET_ERR_PARAM;
        }
    }
    ipaddr_copy(&sock->local_ip, &local_ip);
    sock->local_port = e_ntohs(local->sin_port);
    return NET_OK;
}

net_err_t sock_listen_req_in(func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_listen_t * listen = (sock_listen_t *)&req->listen;
    if (!sock->ops->listen) {
        log_error(LOG_SOCKET, "this function is not implemented");
        return NET_ERR_NOT_SUPPORT;
    }
    return sock->ops->listen(sock, listen->backlog);
}


net_err_t sock_accept_req_in(func_msg_t * api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock_accept_t * accept = (sock_accept_t *)&req->accept;

    if (!sock->ops->accept) {
        log_error(LOG_SOCKET, "this function is not implemented");
        return NET_ERR_NOT_SUPPORT;
    }
    sock_t * client = (sock_t *)0;
    net_err_t err = sock->ops->accept(sock, accept->addr, accept->len, &client);
    if (err < 0) {
        log_error(LOG_SOCKET, "accept error: %d", err);
        return err;
    } else if (err == NET_ERR_NEED_WAIT) {
        if (sock->conn_wait) {
            sock_wait_add(sock->conn_wait, sock->rcv_tmo, req);
        }
    } else {
        x_socket_t * child_socket = socket_alloc();
        if (child_socket == (x_socket_t *)0) {
            log_error(LOG_SOCKET, "no socket");
            return NET_ERR_NONE;
        }
        child_socket->sock = client;
        accept->client = get_index(child_socket);
    }
    return NET_OK;
}



net_err_t sock_destroy_req_in (func_msg_t* api_msg) {
    sock_req_t * req = (sock_req_t *)api_msg->param;
    x_socket_t* s = get_socket(req->sockfd);
    if (!s) {
        log_error(LOG_SOCKET, "param error: socket = %d.", s);
        return NET_ERR_PARAM;
    }
    sock_t* sock = s->sock;
    sock->ops->destroy(sock);
    socket_free(s);
    return NET_OK;
}