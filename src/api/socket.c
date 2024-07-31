#include "log.h"
#include "easy_net_config.h"
#include "sock.h"
#include "socket.h"

int x_socket(int family, int type, int protocol) {
    sock_req_t req;
    req.sockfd = -1;
    req.wait = 0;
    req.create.family = family;
    req.create.type = type;
    req.create.protocol = protocol;
    net_err_t err = exmsg_func_exec(sock_create_req_in, &req);
    if (err < 0) {
        log_error(LOG_SOCKET, "create sock failed: %d.", err);
        return -1;
    }
    return req.sockfd;
}

/**
 * Linux-like api
 * */
ssize_t x_sendto(int sockfd, const void* buf, size_t size, int flags, const struct x_sockaddr* dest, x_socklen_t addr_len) {
    if ((addr_len != sizeof(struct x_sockaddr)) || !size) {
        log_error(LOG_SOCKET, "addr size or len error");
        return -1;
    }
    if (dest->sa_family != AF_INET) {
        log_error(LOG_SOCKET, "family error");
        return -1;
    }

    ssize_t send_size = 0;
    uint8_t * start = (uint8_t *)buf;
    while (size) {
        sock_req_t req;
        req.sockfd = sockfd;
        req.wait = 0;
        req.data.buf = start;
        req.data.len = size;
        req.data.flags = flags;
        req.data.addr = (struct x_sockaddr* )dest;
        req.data.addr_len = &addr_len;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_sendto_req_in, &req);
        if (err < 0) {
            log_error(LOG_SOCKET, "write failed.");
            return -1;
        }
        // if the handler tells us to wait for some time, we should wait for it
        if (req.wait && ((err = sock_wait_enter(req.wait, req.wait_tmo)) < NET_OK)) {
            log_error(LOG_SOCKET, "send failed %d.", err);
            return -1;
        }
        size -= req.data.comp_len;
        send_size += (ssize_t)req.data.comp_len;
        start += req.data.comp_len;
    }
    return send_size;
}



ssize_t x_recvfrom(int sockfd, void* buf, size_t size, int flags, struct x_sockaddr* addr, x_socklen_t* len) {
    if (!size || !len || !addr) {
        log_error(LOG_SOCKET, "addr size or len error");
        return -1;
    }
    while(1){
        sock_req_t req;
        req.sockfd = sockfd;
        req.wait = 0;
        req.data.buf = buf;
        req.data.len = size;
        req.data.comp_len = 0;
        req.data.addr = addr;
        req.data.addr_len = len;
        net_err_t err = exmsg_func_exec(sock_recvfrom_req_in, &req);
        if (err < 0) {
            log_error(LOG_SOCKET, "connect failed:", err);
            return -1;
        }
        // if read any data, return the length of data
        if (req.data.comp_len) {
            return (ssize_t)req.data.comp_len;
        }
        log_info(LOG_SOCKET, "no data, wait for %d ms", req.wait_tmo);
        net_time_t time;
        sys_time_curr(&time);
        err = sock_wait_enter(req.wait, req.wait_tmo);
        int diff_ms = sys_time_goes(&time);
        log_info(LOG_SOCKET, "wait_t %p waited %d ms", req.wait, diff_ms);
        if (err < 0) {
            log_error(LOG_SOCKET, "recv failed %d.", err);
            return -1;
        }
    }
}


int x_setsockopt(int sockfd, int level, int optname, const char * optval, int optlen) {
    if (!optval || !optlen) {
        log_error(LOG_SOCKET, "param error", NET_ERR_PARAM);
        return -1;
    }

    sock_req_t req;
    req.wait = 0;
    req.sockfd = sockfd;
    req.opt.level = level;
    req.opt.optname = optname;
    req.opt.optval = optval;
    req.opt.optlen = optlen;
    net_err_t err = exmsg_func_exec(sock_setsockopt_req_in, &req);
    if (err < 0) {
        log_error(LOG_SOCKET, "setopt:", err);
        return -1;
    }

    return 0;
}


int x_close(int sockfd) {
    sock_req_t req;
    req.wait = 0;
    req.sockfd = sockfd;
    net_err_t err = exmsg_func_exec(sock_close_req_in, &req);
    if (err < 0) {
        log_error(LOG_SOCKET, "try close failed %d, force delete.", err);
        return -1;
    }
    if (req.wait) {
        sock_wait_enter(req.wait, req.wait_tmo);
    }
    // TODO: free tcp_t
    return 0;
}


/**
 * connect is just to set remote address and remote port
 * */
int x_connect(int sockfd, const struct x_sockaddr* addr, x_socklen_t len) {
    if ((len != sizeof(struct x_sockaddr)) || !addr) {
        log_error(LOG_SOCKET, "addr error");
        return -1;
    }
    if (addr->sa_family != AF_INET) {
        log_error(LOG_SOCKET, "family error");
        return -1;
    }
    const struct x_sockaddr_in* addr_in = (const struct x_sockaddr_in*)addr;
    if ((addr_in->sin_addr.s_addr == INADDR_ANY) || !addr_in->sin_port) {
        log_error(LOG_SOCKET, "ip or port is empty");
        return -1;
    }
    sock_req_t req;
    req.wait = 0;
    req.sockfd = sockfd;
    req.conn.addr = addr;
    req.conn.len = len;
    net_err_t err = exmsg_func_exec(sock_connect_req_in, &req);
    if (err < 0) {
        log_error(LOG_SOCKET, "try connect failed: %d", err);
        return -1;
    }
    if (req.wait && ((err = sock_wait_enter(req.wait, req.wait_tmo)) < NET_OK)) {
        log_error(LOG_SOCKET, "connect failed %d.", err);
        return -1;
    }
    return 0;
}



ssize_t x_send(int sockfd, const void* buf, size_t len, int flags) {
    ssize_t send_size = 0;
    uint8_t * start = (uint8_t *)buf;
    while (len) {
        sock_req_t req;
        req.wait = 0;
        req.sockfd = sockfd;
        req.data.buf = start;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_send_req_in, &req);
        if (err < 0) {
            log_error(LOG_SOCKET, "write failed.");
            return -1;
        }
        if (req.wait && ((err = sock_wait_enter(req.wait, req.wait_tmo)) < NET_OK)) {
            log_error(LOG_SOCKET, "send failed %d.", err);
            return -1;
        }

        len -= req.data.comp_len;
        send_size += (ssize_t)req.data.comp_len;
        start += req.data.comp_len;
    }
    return send_size;
}



ssize_t x_recv(int sockfd, void* buf, size_t len, int flags) {
    while (1) {
        sock_req_t req;
        req.wait = 0;
        req.sockfd = sockfd;
        req.data.buf = buf;
        req.data.len = len;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_recv_req_in, &req);
        if (err < 0) {
            log_error(LOG_SOCKET, "rcv failed.:", err);
            return -1;
        }
        if (req.data.comp_len) {
            return (ssize_t)req.data.comp_len;
        }
        err = sock_wait_enter(req.wait, req.wait_tmo);
        if(err == NET_ERR_CLOSED){
            log_warning(LOG_SOCKET, "connection closed by remote");
            return 0;
        }
        if (err < 0) {
            log_error(LOG_SOCKET, "recv failed %d.", err);
            return -1;
        }
    }
}


/**
 * bind is to set local address and local port
 * to limit the scope of the socket
 * */
int x_bind(int sockfd, const struct x_sockaddr* addr, x_socklen_t len) {
    if ((len != sizeof(struct x_sockaddr)) || !addr) {
        log_error(LOG_SOCKET, "addr len error");
        return -1;
    }
    if (addr->sa_family != AF_INET) {
        log_error(LOG_SOCKET, "family error");
        return -1;
    }
    sock_req_t req;
    req.wait = 0;
    req.sockfd = sockfd;
    req.bind.addr = addr;
    req.bind.len = len;
    net_err_t err = exmsg_func_exec(sock_bind_req_in, &req);
    if (err < 0) {
        log_error(LOG_SOCKET, "setopt:", err);
        return -1;
    }
    return 0;
}




int x_accept(int sockfd, struct x_sockaddr* addr, x_socklen_t* len) {
    if (!addr || !len) {
        log_error(LOG_SOCKET, "addr len error");
        return -1;
    }
    while (1) {
        sock_req_t req;
        req.sockfd = sockfd;
        req.wait = 0;
        req.accept.addr = addr;
        req.accept.len = len;
        req.accept.client = -1;
        net_err_t err = exmsg_func_exec(sock_accept_req_in, &req);
        if (err < 0) {
            log_error(LOG_SOCKET, "accept failed: %d ", err);
            return -1;
        }
        if (req.accept.client >= 0) {
            log_info(LOG_SOCKET, "get new connection");
            return req.accept.client;
        }
        if (req.wait && ((err = sock_wait_enter(req.wait, req.wait_tmo)) < NET_OK)) {
            log_error(LOG_SOCKET, "connect failed %d.", err);
            return -1;
        }
    }
}


int x_listen(int sockfd, int backlog) {
    sock_req_t req;
    req.wait = 0;
    req.sockfd = sockfd;
    req.listen.backlog = backlog;
    net_err_t err = exmsg_func_exec(sock_listen_req_in, &req);
    if (err < 0) {
        log_error(LOG_SOCKET, "listen failed: %d", err);
        return -1;
    }
    return 0;
}