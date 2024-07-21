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
        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0) {
            log_error(LOG_SOCKET, "recv failed %d.", err);
            return -1;
        }
    }
}
