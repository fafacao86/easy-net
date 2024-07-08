#include <string.h>
#include "netif_pcap.h"
#include "sys_plat.h"
#include "stack.h"
#include "netif_pcap.h"
#include "log.h"

/**
 * @brief 网络设备初始化
 */
net_err_t init_network_device(void) {
    open_network_interface();
    return NET_OK;
}


int main (void) {
//    init_stack();
//    init_network_device();
//    start_easy_net();
    log_error(LEVEL_ERROR,"Hello, world!");
    while (1) {
        sys_sleep(10);
    }
}
