#include "arp.h"
#include "memory_pool.h"
#include "log.h"
#include "protocols.h"
#include "utils.h"

static arp_entry_t cache_tbl[ARP_CACHE_SIZE];
static memory_pool_t arp_cache_pool;
static list_t cache_list;
static const uint8_t empty_hwaddr[] = {0, 0, 0, 0, 0, 0};

/**
 * initialize ARP cache
 */
static net_err_t cache_init(void) {
    init_list(&cache_list);
    plat_memset(cache_tbl, 0, sizeof(cache_tbl));
    net_err_t err = memory_pool_init(&arp_cache_pool, cache_tbl, sizeof(arp_entry_t), ARP_CACHE_SIZE, LOCKER_NONE);
    if (err < 0) {
        return err;
    }
    return NET_OK;
}

/**
 * init arp module
 */
net_err_t arp_init(void) {
    net_err_t err = cache_init();
    if (err < 0) {
        log_error(LOG_ARP, "arp cache init failed.");
        return err;
    }

    return NET_OK;
}


/**
 * send ARP request to get the MAC address of the target IP address
 */
net_err_t arp_make_request(netif_t* netif, const ipaddr_t* pro_addr) {
    packet_t* packet = packet_alloc(sizeof(arp_pkt_t));
    if (packet == NULL) {
        dbg_dump_ip(LOG_ARP, "allocate arp packet failed. ip:", pro_addr);
        return NET_ERR_NONE;
    }

    packet_set_cont(packet, sizeof(arp_pkt_t));

    arp_pkt_t* arp_packet = (arp_pkt_t*)packet_data(packet);
    arp_packet->htype = e_htons(ARP_HW_ETHER);
    arp_packet->ptype = e_htons(NET_PROTOCOL_IPv4);
    arp_packet->hlen = ETH_HWA_SIZE;
    arp_packet->plen = IPV4_ADDR_SIZE;
    arp_packet->opcode = e_htons(ARP_REQUEST);
    plat_memcpy(arp_packet->send_haddr, netif->hwaddr.addr, ETH_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_packet->send_paddr);
    plat_memcpy(arp_packet->target_haddr, empty_hwaddr, ETH_HWA_SIZE);
    ipaddr_to_buf(pro_addr, arp_packet->target_paddr);

    // broadcast ARP request
    net_err_t err = ether_raw_out(netif, NET_PROTOCOL_ARP, ether_broadcast_addr(), packet);
    if (err < 0) {
        packet_free(packet);
    }
    return err;
}

/**
 * This will be called when the protocol stack opens a new network interface on startup.
 * The purpose of gratuitous ARP is to make an announcement to the LAN
 * that a new device has joined the network.
 * So other devices in the LAN can update their ARP cache,
 * and also detect conflicts, but in this protocol stack, we simply throw an error when conflicts occur.
 * */
net_err_t arp_make_gratuitous(netif_t* netif) {
    log_info(LOG_ARP, "send an gratuitous arp....");

    // gratuitous ARP request
    // target and sender ip address both are the current ip address of the interface
    return arp_make_request(netif, &netif->ipaddr);
}