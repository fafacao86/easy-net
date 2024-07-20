#include "log.h"
#include "easy_net_config.h"
#include "sock.h"
#include "socket.h"

int x_socket(int family, int type, int protocol) {
    sock_req_t req;
    req.sockfd = -1;
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