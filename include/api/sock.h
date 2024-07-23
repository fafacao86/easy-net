#ifndef EASY_NET_SOCK_H
#define EASY_NET_SOCK_H
#include "net_errors.h"
#include "msg_handler.h"

/**
 * for socket type specific operations, polymorphism
 * */
#define SOCK_WAIT_READ         (1 << 0)
#define SOCK_WAIT_WRITE        (1 << 1)
#define SOCK_WAIT_CONN         (1 << 2)
#define SOCK_WAIT_ALL          (SOCK_WAIT_CONN |SOCK_WAIT_READ | SOCK_WAIT_WRITE)

typedef struct _sock_wait_t {
    net_err_t err;                  // result of wait
    int waiting;                    // are there any events to wait for
    sys_sem_t sem;                  // semaphore to wait on
}sock_wait_t;


net_err_t sock_wait_init (sock_wait_t * wait);
void sock_wait_destroy (sock_wait_t * wait);
void sock_wait_add (sock_wait_t * wait, int tmo, struct _sock_req_t * req) ;
net_err_t sock_wait_enter (sock_wait_t * wait, int tmo);
void sock_wait_leave (sock_wait_t * wait, net_err_t err);



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
    net_err_t (*connect)(struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len);
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
    sock_wait_t * snd_wait;
    sock_wait_t * rcv_wait;
    sock_wait_t * conn_wait;
    list_node_t node;
}sock_t;

/**
 * data transfer req
 * */
typedef struct _sock_data_t {
    uint8_t * buf;
    size_t len;
    int flags;
    struct x_sockaddr* addr;
    x_socklen_t * addr_len;
    ssize_t comp_len;              // bytes actually sent/received
}sock_data_t;

// req for connect
typedef struct _sock_conn_t {
    const struct x_sockaddr* addr;
    x_socklen_t len;
}sock_conn_t;


typedef struct _sock_create_t {
    int family;
    int protocol;
    int type;
}sock_create_t;


typedef struct _sock_opt_t {
    int level;
    int optname;
    const char * optval;
    int optlen;
}sock_opt_t;



/**
 * API Request structure
 */
typedef struct _sock_req_t {
    int sockfd;
    // the responder might want the caller to wait for some events
    sock_wait_t * wait;
    int wait_tmo;
    union {
        sock_create_t create;
        sock_data_t data;
        sock_opt_t opt;
        sock_conn_t conn;
    };
}sock_req_t;

net_err_t sock_create_req_in(func_msg_t* api_msg);
net_err_t sock_sendto_req_in (func_msg_t * api_msg);
net_err_t sock_recvfrom_req_in(func_msg_t * api_msg);
net_err_t sock_close_req_in (func_msg_t* api_msg);
net_err_t sock_connect_req_in (func_msg_t* api_msg);

net_err_t sock_connect(sock_t* sock, const struct x_sockaddr* addr, x_socklen_t len);
net_err_t sock_init(sock_t* sock, int family, int protocol, const sock_ops_t * ops);
net_err_t socket_init(void);
void sock_uninit (sock_t * sock);
net_err_t sock_setsockopt_req_in(func_msg_t * api_msg);
net_err_t sock_setopt(struct _sock_t* s,  int level, int optname, const char * optval, int optlen);
void sock_wakeup (sock_t * sock, int type, int err);
#endif //EASY_NET_SOCK_H
