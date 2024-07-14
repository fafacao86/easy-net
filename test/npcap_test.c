#include "netif_pcap.h"
#include "sys_plat.h"
#include "stack.h"
#include "log.h"
#include "testcase.h"
pcap_data_t netdev0_data = { .ip = netdev0_phy_ip, .hwaddr = netdev0_hwaddr };

net_err_t init_network_device(void) {
    netif_t* netif = netif_open("netif 0", &netdev_ops, &netdev0_data);
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
    packet_t * packet = packet_alloc(32);
    packet_fill(packet, 0x53, 32);
    ipaddr_t dest;
    ipaddr_from_str(&dest, friend0_ip);
    netif_out(netif, &dest, packet);
    return NET_OK;
}


int main (void) {
    init_stack();
    test_timer();
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
