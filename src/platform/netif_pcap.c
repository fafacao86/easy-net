#if defined(NET_DRIVER_PCAP)
#include "net_plat.h"

/**
 * 数据包接收线程，不断地收数据包
 */
void recv_thread(void* arg) {
    plat_printf("recv thread start running...\n");
    while (1) {
        sys_sleep(1);
    }
}

/**
 * 模拟硬件发送线程
 */
void send_thread(void* arg) {
    plat_printf("send thread start running...\n");
    while (1) {
        sys_sleep(1);
    }
}

/**
 * pcap设备打开
 * @param netif 打开的接口
 * @param driver_data 传入的驱动数据
 */
net_err_t open_network_interface(void) {
    sys_thread_t rt = sys_thread_create(recv_thread, (void *)0);
    if (rt == NULL) {
        plat_printf("create recv thread failed!\n");
        return NET_ERR_SYS;
    }
    sys_thread_t st = sys_thread_create(send_thread, (void *)0);
    if (st == NULL) {
        plat_printf("create send thread failed!\n");
        return NET_ERR_SYS;
    }
    return NET_OK;
}
#endif