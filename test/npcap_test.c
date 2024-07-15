#include "netif_pcap.h"
#include "sys_plat.h"
#include "stack.h"
#include "log.h"
#include "testcase.h"

pcap_data_t netdev0_data = { .ip = netdev0_phy_ip, .hwaddr = netdev0_hwaddr };
static netif_t* netif = NULL;
net_err_t init_network_device(void) {
    netif = netif_open("netif 0", &netdev_ops, &netdev0_data);
    if (!netif) {
        fprintf(stderr, "netif open failed.");
        exit(-1);
    }
    ipaddr_t ip, mask, gw;
    ipaddr_from_str(&ip, netdev0_ip);
    ipaddr_from_str(&mask, netdev0_mask);
    ipaddr_from_str(&gw, netdev0_gw);
    netif_set_addr(netif, &ip, &mask, &gw);
    netif_set_active(netif);
    return NET_OK;
}


int main (void) {
    init_stack();
    //test_timer();
    test_arp(netif);
    init_network_device();
    start_easy_net();

//    init_network_device();
    //start_easy_net();
    //test_logging();
    //test_list();
    //test_memory_pool();
    //test_msg_handler();
    //test_packet_buffer();
    while(1)sys_sleep(1000);
}
