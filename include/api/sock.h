#ifndef EASY_NET_SOCK_H
#define EASY_NET_SOCK_H
#include "net_errors.h"
#include "msg_handler.h"

/**
 * for socket type specific operations
 * */


struct _sock_t;
struct x_sockaddr;

typedef int x_socklen_t;

/**
 * interfaces for socket operations, like polymorphism
 */
typedef struct _sock_ops_t {
    net_err_t (*close)(struct _sock_t* s);
    net_err_t (*sendto)(struct _sock_t * s, const void* buf, size_t len, int flags,
                        const struct x_sockaddr* dest, x_socklen_t dest_len, ssize_t * result_len);
    net_err_t(*recvfrom)(struct _sock_t* s, void* buf, size_t len, int flags,
                         struct x_sockaddr* src, x_socklen_t * addr_len, ssize_t * result_len);
    net_err_t (*setopt)(struct _sock_t* s,  int level, int optname, const char * optval, int optlen);
    void (*destroy)(struct _sock_t *s);
}sock_ops_t;

/**
 * sock
 */
typedef struct _sock_t {
    ipaddr_t local_ip;				// src ip
    ipaddr_t remote_ip;				// dst ip
    uint16_t local_port;			// src port
    uint16_t remote_port;			// dst port
    const sock_ops_t* ops;			// specific operations for this type of socket
    int family;                     // protocol family
    int protocol;					// protocol type
    int err;						// err code of last operation
    int rcv_tmo;					// ms
    int snd_tmo;					// ms

    list_node_t node;
}sock_t;



typedef struct _sock_create_t {
    int family;
    int protocol;
    int type;
}sock_create_t;

/**
 * API Request structure
 */
typedef struct _sock_req_t {
    int sockfd;
    union {
        sock_create_t create;
    };
}sock_req_t;

net_err_t sock_create_req_in(func_msg_t* api_msg);
net_err_t sock_init(sock_t* sock, int family, int protocol, const sock_ops_t * ops);
net_err_t socket_init(void);


#endif //EASY_NET_SOCK_H
