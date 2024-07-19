#include "stack.h"
#include "msg_handler.h"
#include "loop.h"
#include "ether.h"
#include "utils.h"
#include "timer.h"
#include "arp.h"
#include "icmpv4.h"
#include "ipv4.h"
#include "sock.h"

/**
 * initialization of the protocol stack
 */
net_err_t init_stack(void) {
    net_plat_init();
    net_timer_init();
    utils_init();
    packet_buffer_init();
    netif_init();
    ether_init();
    arp_init();
    ipv4_init();
    icmpv4_init();
    init_msg_handler();
    socket_init();
    //loop_init();
    return NET_OK;
}

/**
 * start the protocol stack
 */
net_err_t start_easy_net(void) {
    //init_msg_handler();
    net_err_t err =  start_msg_handler();
    if (err!= NET_OK) {
        return err;
    }
    return NET_OK;
}
