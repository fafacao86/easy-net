#ifndef EASY_NET_MSG_HANDLER_H
#define EASY_NET_MSG_HANDLER_H

#include "list.h"
#include "netif.h"

typedef struct exmsg_t {
    // 消息类型
    enum {
        NET_EXMSG_NETIF_IN,             // 网络接口数据消息
    }type;

    list_node_t temp;           // 临时使用
}exmsg_t;


net_err_t init_msg_handler (void);
net_err_t start_msg_handler (void);
net_err_t exmsg_netif_in(netif_t* netif);
#endif
