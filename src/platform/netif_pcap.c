#if defined(NET_DRIVER_PCAP)
#include "net_plat.h"
#include "netif.h"
#include "netif_pcap.h"
#include "log.h"

/**
 * Listen on physical network interface and receive packets.
 * When a packet is received, put a new message o handler thread via msg queue.
 * When failed, free the packet. When success, handler thread will free the packet.
 * */
void recv_thread(void* arg) {
    plat_printf("recv thread start running...\n");
    netif_t* netif = (netif_t*)arg;
    pcap_t* pcap = (pcap_t*)netif->ops_data;
    while (1) {
        // 1 - success, 0 - no packets，others - error
        struct pcap_pkthdr* pkthdr;
        const uint8_t* pkt_data;
        if (pcap_next_ex(pcap, &pkthdr, &pkt_data) != 1) {
            continue;
        }

        // convert pcap_pkthdr to packet_t
        packet_t* packet = packet_alloc(pkthdr->len);
        if (packet == (packet_t*)0) {
            log_warning(LOG_NETIF, "packet == NULL");
            continue;
        }
        packet_write(packet, (uint8_t*)pkt_data, pkthdr->len);
        if (netif_put_in(netif, packet, 0) < 0) {
            log_warning(LOG_NETIF, "netif %s in_q full", netif->name);
            packet_free(packet);
            continue;
        }
    }
}

/**
 * Polling the out queue of network interface and send packets.
 * Packets are put by handler
 * */
void send_thread(void* arg) {
    plat_printf("send thread start running...\n");
    static uint8_t rw_buffer[NETIF_MTU+14];
    netif_t* netif = (netif_t*)arg;
    pcap_t* pcap = (pcap_t*)netif->ops_data;

    while (1) {
        packet_t* packet = netif_get_out(netif, 0);
        if (packet == (packet_t*)0) {
            continue;
        }

        int total_size = packet->total_size;
        plat_memset(rw_buffer, 0, sizeof(rw_buffer));
        packet_read(packet, rw_buffer, total_size);
        packet_free(packet);
        if (pcap_inject(pcap, rw_buffer, total_size) == -1) {
            log_warning(LOG_NETIF, "pcap send: send packet failed!:%s\n", pcap_geterr(pcap));
            log_warning(LOG_NETIF, "pcap send: pcaket size %d\n", total_size);
            continue;
        }
    }
}


//net_err_t open_network_interface(void) {
//    sys_thread_t rt = sys_thread_create(recv_thread, (void *)0);
//    if (rt == NULL) {
//        plat_printf("create recv thread failed!\n");
//        return NET_ERR_SYS;
//    }
//    sys_thread_t st = sys_thread_create(send_thread, (void *)0);
//    if (st == NULL) {
//        plat_printf("create send thread failed!\n");
//        return NET_ERR_SYS;
//    }
//    return NET_OK;
//}


static net_err_t netif_pcap_open(struct _netif_t* netif, void* ops_data) {
    pcap_data_t* dev_data = (pcap_data_t*)ops_data;
    pcap_t * pcap = pcap_device_open(dev_data->ip, dev_data->hwaddr);
    if (pcap == (pcap_t*)0) {
        log_error(LOG_NETIF, "pcap open failed! name: %s\n", netif->name);
        return NET_ERR_IO;
    }
    netif->ops_data = pcap;

    netif->type = NETIF_TYPE_ETHER;
    netif->mtu = NETIF_MTU;
    netif_set_hwaddr(netif, dev_data->hwaddr, NETIF_HWADDR_SIZE);

    sys_thread_create(send_thread, netif);
    sys_thread_create(recv_thread, netif);
    return NET_OK;
}


static void netif_pcap_close(struct _netif_t* netif) {
    pcap_t* pcap = (pcap_t*)netif->ops_data;
    pcap_close(pcap);

    // todo: 关闭线程
}


static net_err_t netif_pcap_transmit (struct _netif_t* netif) {
    // do nothing, because the sender thread will send the packets
    return NET_OK;
}


const netif_ops_t netdev_ops = {
        .open = netif_pcap_open,
        .close = netif_pcap_close,
        .transmit = netif_pcap_transmit,
};

#endif