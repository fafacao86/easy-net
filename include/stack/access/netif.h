#ifndef EASY_NET_NETIF_H
#define EASY_NET_NETIF_H

#include <sys_plat.h>
#include "ipaddr.h"
#include "fixed_queue.h"
#include "easy_net_config.h"
#include "packet_buffer.h"

/**
 * hardware address
 * usually mac address
 */
typedef struct _netif_hwaddr_t {
    uint8_t len;                            // address len
    uint8_t addr[NETIF_HWADDR_SIZE];        // address
}netif_hwaddr_t;

/**
 * operations supported by network interface
 */
struct _netif_t;

typedef struct _netif_ops_t {
    net_err_t(*open) (struct _netif_t* netif, void * data);
    void (*close) (struct _netif_t* netif);

    net_err_t (*transmit)(struct _netif_t* netif);
}netif_ops_t;

/**
 * interface type
 */
typedef enum netif_type_t {
    NETIF_TYPE_NONE = 0,                // none
    NETIF_TYPE_ETHER,                   // ethernet
    NETIF_TYPE_LOOP,                    // loop back
    NETIF_TYPE_SIZE,
}netif_type_t;

/**
 * network interface
 */
typedef struct _netif_t {
    char name[NETIF_NAME_SIZE];             // network interface name

    netif_hwaddr_t hwaddr;                  // hardware address
    ipaddr_t ipaddr;                        // ip address
    ipaddr_t netmask;                       // mask
    ipaddr_t gateway;                       // gateway

    enum {                                  // network interface state
        NETIF_CLOSED,                       // closed
        NETIF_OPENED,                       // opened
        NETIF_ACTIVE,                       // active
    }state;

    netif_type_t type;                      // interface type
    int mtu;                                // Maximum Transfer Unit

    const netif_ops_t* ops;                 // operations supported by network interface
    void* ops_data;                         // data for operations

    list_node_t node;                       // multiple network interfaces support

    fixed_queue_t in_q;                            // input queue
    void * in_q_buf[NETIF_INQ_SIZE];
    fixed_queue_t out_q;                           // output queue
    void * out_q_buf[NETIF_OUTQ_SIZE];

    // some statistics member variables can be added here
}netif_t;

net_err_t netif_init(void);
netif_t* netif_open(const char* dev_name, const netif_ops_t* driver, void* driver_data);
net_err_t netif_set_addr(netif_t* netif, ipaddr_t* ip, ipaddr_t* netmask, ipaddr_t* gateway);
net_err_t netif_set_hwaddr(netif_t* netif, const uint8_t* hwaddr, int len);
net_err_t netif_set_active(netif_t* netif);
net_err_t netif_set_deactive(netif_t* netif);
void netif_set_default (netif_t * netif);
net_err_t netif_close(netif_t* netif);

net_err_t netif_out(netif_t* netif, ipaddr_t* ipaddr, packet_t* buf);

// functions for input and output queue of a network interface
net_err_t netif_put_in(netif_t* netif, packet_t* packet, int tmo);
net_err_t netif_put_out(netif_t * netif, packet_t * buf, int tmo);
packet_t * netif_get_in(netif_t* netif, int tmo);
packet_t* netif_get_out(netif_t * netif, int tmo);



#endif //EASY_NET_NETIF_H
