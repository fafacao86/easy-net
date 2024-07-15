#include "arp.h"
#include "memory_pool.h"
#include "log.h"
#include "protocols.h"
#include "utils.h"

static arp_entry_t cache_tbl[ARP_CACHE_SIZE];
static memory_pool_t arp_cache_pool;
static list_t cache_list;
static const uint8_t empty_hwaddr[] = {0, 0, 0, 0, 0, 0};

#if LOG_DISP_ENABLED(LOG_ARP)
void display_arp_entry(arp_entry_t* entry) {
    plat_printf("%d: ", (int)(entry - cache_tbl));       // 序号
    dump_ip_buf(" ip:", entry->paddr);
    dump_mac(" mac:", entry->haddr);
    plat_printf(" tmo: %d, retry: %d, %s, buf: %d\n",
                entry->tmo, entry->retry, entry->state == NET_ARP_RESOLVED ? "stable" : "pending",
                list_count(&entry->buf_list));
}


void display_arp_tbl(void) {
    plat_printf("\n------------- ARP table start ---------- \n");

    arp_entry_t* entry = cache_tbl;
    for (int i = 0; i < ARP_CACHE_SIZE; i++, entry++) {
        if ((entry->state != NET_ARP_FREE) && (entry->state != NET_ARP_RESOLVED)) {
            continue;
        }

        display_arp_entry(entry);
    }

    plat_printf("------------- ARP table end ---------- \n");
}


static void arp_pkt_display(arp_pkt_t* packet) {
    uint16_t opcode = e_ntohs(packet->opcode);

    plat_printf("--------------- arp start ------------------\n");
    plat_printf("    htype:%x\n", e_ntohs(packet->htype));
    plat_printf("    pype:%x\n", e_ntohs(packet->ptype));
    plat_printf("    hlen: %x\n", packet->hlen);
    plat_printf("    plen:%x\n", packet->plen);
    plat_printf("    type:%04x  ", opcode);
    switch (opcode) {
        case ARP_REQUEST:
            plat_printf("request\n");
            break;;
        case ARP_REPLY:
            plat_printf("reply\n");
            break;
        default:
            plat_printf("unknown\n");
            break;
    }
    dump_ip_buf("    sender:", packet->send_paddr);
    dump_mac("  mac:", packet->send_haddr);
    plat_printf("\n");
    dump_ip_buf("    target:", packet->target_paddr);
    dump_mac("  mac:", packet->target_haddr);
    plat_printf("\n");
    plat_printf("--------------- arp end ------------------ \n");
}

#else
#define display_arp_entry(entry)
#define display_arp_tbl()
#define arp_pkt_display(packet)
#endif


/**
 * free all packets pending on the ARP entry
 */
static void cache_clear_all(arp_entry_t* entry) {
    log_info(LOG_ARP, "clear %d packet:", list_count(&entry->buf_list));
    dbg_dump_ip_buf(LOG_ARP, "ip:", entry->paddr);

    list_node_t * first;
    while ((first = list_remove_first(&entry->buf_list))) {
        packet_t* packet = list_entry(first, packet_t, node);
        packet_free(packet);
    }
}


static arp_entry_t* cache_alloc(int force) {
    arp_entry_t* entry = memory_pool_alloc(&arp_cache_pool, -1);
    if (!entry && force) {
        // if there is no available entry, evict the tail of the cache list
        list_node_t * node = list_remove_last(&cache_list);
        if (!node) {
            log_warning(LOG_ARP, "allocate arp entry failed");
            return (arp_entry_t*)0;
        }

        log_info(LOG_ARP, "allocate an arp entry from cache list");

        entry = list_entry(node, arp_entry_t, node);
        cache_clear_all(entry);
    }

    if (entry) {
        plat_memset(entry, 0, sizeof(arp_entry_t));
        entry->state = NET_ARP_FREE;
        list_node_init(&entry->node);
        init_list(&entry->buf_list);
    }
    return entry;
}

/**
 * free an ARP cache entry
 */
static void cache_free(arp_entry_t *entry) {
    // free all packets pending on the ARP entry
    cache_clear_all(entry);
    list_remove(&cache_list, &entry->node);
    memory_pool_free(&arp_cache_pool, entry);
}


static arp_entry_t* cache_find(uint8_t* ip) {
    list_node_t* node;
    list_for_each(node, &cache_list) {
        arp_entry_t* entry = list_entry(node, arp_entry_t, node);
        if (plat_memcmp(ip, entry->paddr, IPV4_ADDR_SIZE) == 0) {
            // remove from list and insert to the head, take advantage of the time locality
            list_remove(&cache_list, node);
            list_insert_first(&cache_list, node);
            return entry;
        }
    }
    return (arp_entry_t*)0;
}


static net_err_t cache_send_all (arp_entry_t* entry) {
    log_info(LOG_ARP, "send %d packet:", list_count(&entry->buf_list));
    dbg_dump_ip_buf(LOG_ARP, "ip:", entry->paddr);
    list_node_t * first;
    while ((first = list_remove_first(&entry->buf_list))) {
        packet_t* buf = list_entry(first, packet_t, node);
        net_err_t err = ether_raw_out(entry->netif, NET_PROTOCOL_IPv4, entry->haddr, buf);
        if (err < 0) {
            packet_free(buf);
        }
    }
    return  NET_OK;
}


static void cache_entry_set(arp_entry_t* entry, const uint8_t* hwaddr,
                            uint8_t* proaddr, netif_t* netif, int state) {
    plat_memcpy(entry->haddr, hwaddr, ETH_HWA_SIZE);
    plat_memcpy(entry->paddr, proaddr, IPV4_ADDR_SIZE);
    entry->state = state;
    entry->netif = netif;
    entry->tmo = 0;
    entry->retry = 0;
}


static net_err_t cache_insert(netif_t* netif, uint8_t* pro_addr, uint8_t* hw_addr, int force) {
    arp_entry_t* entry = cache_find(pro_addr);
    if (!entry) {
        entry = cache_alloc(force);
        if (!entry) {
            dbg_dump_ip_buf(LOG_ARP, "alloc failed! sender ip:", pro_addr);
            return NET_ERR_NONE;
        }
        cache_entry_set(entry, hw_addr, pro_addr, netif, NET_ARP_RESOLVED);
        list_insert_first(&cache_list, &entry->node);
        dbg_dump_ip_buf(LOG_ARP, "insert an entry,sender ip:", pro_addr);
    }
    else {
        dbg_dump_ip_buf(LOG_ARP, "update arp entry, sender ip:", pro_addr);
        cache_entry_set(entry, hw_addr, pro_addr, netif, NET_ARP_RESOLVED);
        if (list_first(&cache_list) != &entry->node) {
            list_remove(&cache_list, &entry->node);
            list_insert_first(&cache_list, &entry->node);
        }
        net_err_t err = cache_send_all(entry);
        if (err < 0) {
            log_error(LOG_ARP, "send packet in entry failed. err = %d", err);
            return err;
        }
    }
    display_arp_tbl();
    return NET_OK;
}


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
 * Send ARP request to get the MAC address of the target IP address
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
    arp_pkt_display(arp_packet);

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



static net_err_t validate_arp_packet(arp_pkt_t* arp_packet, uint16_t size, netif_t* netif) {
    if (size < sizeof(arp_pkt_t)) {
        log_warning(LOG_ARP, "packet size error: %d < %d", size, (int)sizeof(arp_pkt_t));
        return NET_ERR_SIZE;
    }

    // check supported hardware type and protocol type
    if ((e_ntohs(arp_packet->htype) != ARP_HW_ETHER) ||
        (arp_packet->hlen != ETH_HWA_SIZE) ||
        (e_ntohs(arp_packet->ptype) != NET_PROTOCOL_IPv4) ||
        (arp_packet->plen != IPV4_ADDR_SIZE)) {
        log_warning(LOG_ARP, "packet incorrect");
        return NET_ERR_NOT_SUPPORT;
    }

    // there might be some other type of ARP packet such as RARP,
    // but we only support request and reply
    uint32_t opcode = e_ntohs(arp_packet->opcode);
    if ((opcode != ARP_REQUEST) && (opcode != ARP_REPLY)) {
        log_warning(LOG_ARP, "unknown opcode=%d", arp_packet->opcode);
        return NET_ERR_NOT_SUPPORT;
    }
    return NET_OK;
}


net_err_t arp_make_reply(netif_t* netif, packet_t* packet) {
    arp_pkt_t* arp_packet = (arp_pkt_t*)packet_data(packet);

    // swap the sender and target address
    arp_packet->opcode = e_htons(ARP_REPLY);
    plat_memcpy(arp_packet->target_haddr, arp_packet->send_haddr, ETH_HWA_SIZE);
    plat_memcpy(arp_packet->target_paddr, arp_packet->send_paddr, IPV4_ADDR_SIZE);
    plat_memcpy(arp_packet->send_haddr, netif->hwaddr.addr, ETH_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_packet->send_paddr);
    arp_pkt_display(arp_packet);
    return ether_raw_out(netif, NET_PROTOCOL_ARP, arp_packet->target_haddr, packet);
}



/**
 * process incoming ARP packet
 * only handles uni-cast ARP
 * and only handles arp request to this netif, and not scan other netifs on this host
 */
net_err_t arp_in(netif_t* netif, packet_t* packet) {
    log_info(LOG_ARP, "arp in");
    net_err_t err = packet_set_cont(packet, sizeof(arp_pkt_t));
    if (err < 0) {
        return err;
    }
    arp_pkt_t * arp_packet = (arp_pkt_t*)packet_data(packet);
    if (validate_arp_packet(arp_packet, packet->total_size, netif) != NET_OK) {
        return err;
    }
    arp_pkt_display(arp_packet);
    ipaddr_t target_ip;
    ipaddr_from_buf(&target_ip, arp_packet->target_paddr);
    if (ipaddr_is_equal(&target_ip, &netif->ipaddr)) {
        log_info(LOG_ARP, "received an arp for me, force update.");

        // the ARP packet might be the reply for us, or might be a request for us,
        // we can cache it directly to optimize the performance
        cache_insert(netif, arp_packet->send_paddr, arp_packet->send_haddr, 1);

        if (e_ntohs(arp_packet->opcode) == ARP_REQUEST) {
            log_info(LOG_ARP, "arp is request. try to send reply");
            return arp_make_reply(netif, packet);
        }
    } else {
        log_info(LOG_ARP, "received an arp not for me, try to update.");

        // we can try to update the ARP cache, but we don't care if the ARP packet is not for us
        cache_insert(netif, arp_packet->send_paddr, arp_packet->send_haddr, 0);
    }

    // don't forget to free the packet, because we return NET_OK,
    // then we are responsible for freeing the packet
    packet_free(packet);
    return NET_OK;
}


/**
 * If there is no ARP entry for the IP address, then send ARP request, and make a waiting entry.
 * put the packet into the waiting list
 * If there is an ARP entry for the IP address, then check the state of the entry.
 * If the entry is resolved, then send the packets out.
 * If the entry is waiting, then insert the packet into the waiting list.
 * */
net_err_t arp_resolve(netif_t* netif, const ipaddr_t* ipaddr, packet_t* packet) {
    uint8_t pro_addr[IPV4_ADDR_SIZE];
    ipaddr_to_buf(ipaddr, pro_addr);

    arp_entry_t * entry = cache_find(pro_addr);
    if (entry) {
        log_info(LOG_ARP, "found an arp entry.");

        // if the entry is resolved, then send the packet out directly
        if (entry->state == NET_ARP_RESOLVED) {
            net_err_t err = ether_raw_out(entry->netif, NET_PROTOCOL_IPv4, entry->haddr, packet);
            return err;
        }

        // if the entry is pending, then insert the packet into the waiting list
        // when the ARP reply comes back, worker thread will send the packets out in cache_insert()
        if (list_count(&entry->buf_list) <= ARP_MAX_PKT_WAIT) {
            log_info(LOG_ARP, "insert packet to arp entry");
            list_insert_first(&entry->buf_list, &packet->node);
            return NET_OK;
        } else {
            log_warning(LOG_ARP, "too many waiting. ignore it");
            return NET_ERR_FULL;
        }
    }  else {
        dbg_dump_ip(LOG_ARP, "make arp request, ip:", ipaddr);

        // if there is no ARP entry for the IP address
        entry = cache_alloc(1);
        if (entry == (arp_entry_t*)0) {
            log_error(LOG_ARP, "alloc arp failed.");
            return NET_ERR_MEM;
        }

        cache_entry_set(entry, empty_hwaddr, pro_addr, netif, NET_ARP_WAITING);
        list_insert_first(&cache_list, &entry->node);
        log_info(LOG_ARP, "insert packet to arp");
        list_insert_last(&entry->buf_list, &packet->node);

        display_arp_tbl();
        // send ARP request to get the MAC address of the target IP address
        return arp_make_request(netif, ipaddr);
    }
}
