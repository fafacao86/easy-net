#include "netif_pcap.h"
#include "sys_plat.h"
#include "stack.h"
#include "log.h"
#include "testcase.h"

net_err_t init_network_device(void) {
    open_network_interface();
    return NET_OK;
}


int main (void) {
    init_stack();
//    init_network_device();
    start_easy_net();
    //test_logging();
    //test_list();
    //test_memory_pool();
    test_msg_handler();
}
