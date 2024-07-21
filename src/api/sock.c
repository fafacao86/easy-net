#include "sys_plat.h"
#include "msg_handler.h"
#include "log.h"
#include "raw.h"
#include "socket.h"
#include "sock.h"
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
            [SOCK_RAW] = { .protocol = 0, .create = raw_create,}
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
    if (wait->waiting > 0) {
        wait->waiting--;
        wait->err = err;
        sys_sem_notify(wait->sem);
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
        log_error(LOG_SOCKET, "unknow level: %d", level);
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
