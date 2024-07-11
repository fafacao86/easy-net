#ifndef EASY_NET_MSG_HANDLER_H
#define EASY_NET_MSG_HANDLER_H

#include "list.h"
#include "netif.h"

typedef struct _msg_netif_t {
    netif_t* netif;
} msg_netif_t;



typedef struct exmsg_t {
    enum {
        NET_EXMSG_NETIF_IN,             // message from network interface
    }type;

    union {
        msg_netif_t netif;
    };
//    list_node_t temp;           // make sure the size of exmsg_t is greater than list_node_t
}exmsg_t;


net_err_t init_msg_handler (void);
net_err_t start_msg_handler (void);
net_err_t handler_netif_in(netif_t* netif);
#endif
