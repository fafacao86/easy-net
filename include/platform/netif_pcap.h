#ifndef NETIF_PCAP_H
#define NETIF_PCAP_H
#include "net_errors.h"
#include "sys_plat.h"
#include "netif.h"

net_err_t open_network_interface(void);

/**
 * data passed to netif_open() for pcap interface
 * */
typedef struct _pcap_data_t {
    const char* ip;
    const uint8_t* hwaddr;
}pcap_data_t;

/**
 * ops for pcap interface
 * */
extern const netif_ops_t netdev_ops;

#endif // NETIF_PCAP_H

