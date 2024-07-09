#include "stack.h"
#include "msg_handler.h"

/**
 * initialization of the protocol stack
 */
net_err_t init_stack(void) {
    return NET_OK;
}

/**
 * start the protocol stack
 */
net_err_t start_easy_net(void) {
    init_msg_handler();
    net_err_t err =  start_msg_handler();
    if (err!= NET_OK) {
        return err;
    }
    return NET_OK;
}
