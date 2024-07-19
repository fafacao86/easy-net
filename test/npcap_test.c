#include "netif_pcap.h"
#include "sys_plat.h"
#include "stack.h"
#include "log.h"
#include "testcase.h"
#include "src/stack/app/ping/ping.h"

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

void show_help (void) {
    printf("--------------- cmd list ------------------ \n");
    printf("1.ping dest(ip or name)\n");
}

int main (void) {
    init_stack();
    //test_timer();
    init_network_device();
    start_easy_net();
    //test_arp(netif);
    //test_ipv4();
//    init_network_device();
    //start_easy_net();
    //test_logging();
    //test_list();
    //test_memory_pool();
    //test_msg_handler();
    //test_packet_buffer();
    int arg = 0x1234;
    exmsg_func_exec(test_func, (void *)&arg);
    char cmd[32], param[32];
    while (1) {
        show_help();
        printf(">>");
        scanf("%s %s", cmd, param);
        ping_t p;
        if (strcmp(cmd, "ping") == 0) {
            ping_run(&p, param, 4, 1000, 1000);
        }
    }
}
