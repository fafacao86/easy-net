#ifndef EASY_NET_MSG_HANDLER_H
#define EASY_NET_MSG_HANDLER_H

#include "list.h"
#include "netif.h"

typedef struct _msg_netif_t {
    netif_t* netif;
} msg_netif_t;
/**
 *function type, takes in a pointer to a func_msg_t struct and returns a net_err_t
 * */
typedef net_err_t(*exmsg_func_t)(struct _func_msg_t* msg);


typedef struct _func_msg_t {
    sys_thread_t thread;        // caller thread
    exmsg_func_t func;         // this is a function pointer executed by worker thread
    void* param;            // parameter for the function
    net_err_t err;          // err

    sys_sem_t wait_sem;     // semaphore to wait for the function to finish
}func_msg_t;


typedef struct exmsg_t {
    enum {
        NET_EXMSG_NETIF_IN,             // message from network interface
        NET_EXMSG_FUN,                  // message from API function call
    }type;

    union {
        msg_netif_t netif;
        func_msg_t* func_msg;
    };
//    list_node_t temp;           // make sure the size of exmsg_t is greater than list_node_t
}exmsg_t;


net_err_t init_msg_handler (void);
net_err_t start_msg_handler (void);
net_err_t handler_netif_in(netif_t* netif);
net_err_t exmsg_func_exec(exmsg_func_t func, void* param);
net_err_t test_func (struct _func_msg_t* msg);

#endif
