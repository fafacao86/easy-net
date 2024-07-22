#include "net_errors.h"
#include "ipv4.h"
#include "log.h"
#include "utils.h"
#include "protocols.h"
#include "icmpv4.h"
#include "memory_pool.h"
#include "timer.h"
#include "raw.h"

static uint16_t packet_id = 0;                  // incremental id for ipv4 packet

// for fragmentation
static ip_frag_t frag_array[IP_FRAGS_MAX_NR];
static memory_pool_t frag_pool;                    // memory pool for ip_frag_t
static list_t frag_list;                        // fragmented packets list
static net_timer_t frag_timer;

static list_t rt_list;                          // routing table list
static memory_pool_t rt_mblock;
static rentry_t rt_table[IP_RTABLE_SIZE];


static inline uint16_t get_frag_start(ipv4_pkt_t* pkt) {
    return pkt->hdr.offset * 8;
}

static inline int get_data_size(ipv4_pkt_t* pkt) {
    return pkt->hdr.total_len - ipv4_hdr_size(pkt);
}

/**
 * each fragmented packet is wrapped in a ip header, so we need to calculate the actual data size
 * */
static inline uint16_t get_frag_end(ipv4_pkt_t* pkt) {
    return get_frag_start(pkt) + get_data_size(pkt);
}


#if LOG_DISP_ENABLED(LOG_IP)
void rt_nlist_display(void) {
        plat_printf("Route table:\n");

        for (int i = 0, idx = 0; i < IP_RTABLE_SIZE; i++) {
            rentry_t* entry = rt_table + i;
            if (entry->netif) {
                plat_printf("%d: ", idx++);
                dbg_dump_ip(LOG_IP, "net:", &entry->net);
                plat_printf("\t");
                dbg_dump_ip(LOG_IP, "mask:", &entry->mask);
                plat_printf("\t");
                dbg_dump_ip(LOG_IP, "next_hop:", &entry->next_hop);
                plat_printf("\t");
                plat_printf("if: %s", entry->netif->name);
                plat_printf("\n");
            }
        }
}

static void display_ip_frags(void) {
    list_node_t *f_node, * p_node;
    int f_index = 0, p_index = 0;

    plat_printf("DBG_IP frags:");
    for (f_node = list_first(&frag_list); f_node; f_node = list_node_next(f_node)) {
        ip_frag_t* frag = list_entry(f_node, ip_frag_t, node);
        plat_printf("[%d]:\n", f_index++);
        dump_ip_buf("\tip:", frag->ip.a_addr);
        plat_printf("\tid: %d\n", frag->id);
        plat_printf("\ttmo: %d\n", frag->tmo);
        plat_printf("\tbufs: %d\n", list_count(&frag->buf_list));
        plat_printf("\tbufs:\n");
        list_for_each(p_node, &frag->buf_list) {
            packet_t * buf = list_entry(p_node, packet_t, node);
            ipv4_pkt_t* pkt = (ipv4_pkt_t *)packet_data(buf);

            plat_printf("\t\tB%d[%d - %d], ", p_index++, get_frag_start(pkt), get_frag_end(pkt) - 1);
        }
        plat_printf("\n");
    }
    plat_printf("");
}


static void display_ip_packet(ipv4_pkt_t* pkt) {
    ipv4_hdr_t* ip_hdr = (ipv4_hdr_t*)&pkt->hdr;

    plat_printf("--------------- ip ------------------ \n");
    plat_printf("    Version:%d\n", ip_hdr->version);
    plat_printf("    Header len:%d bytes\n", ipv4_hdr_size(pkt));
    plat_printf("    Totoal len: %d bytes\n", ip_hdr->total_len);
    plat_printf("    Id:%d\n", ip_hdr->id);
    plat_printf("    Frag offset: 0x%04x\n", ip_hdr->offset);
    plat_printf("    More frag: %d\n", ip_hdr->more);
    plat_printf("    TTL: %d\n", ip_hdr->ttl);
    plat_printf("    Protocol: %d\n", ip_hdr->protocol);
    plat_printf("    Header checksum: 0x%04x\n", ip_hdr->hdr_checksum);
    dbg_dump_ip_buf(LOG_IP, "    src ip:", ip_hdr->dest_ip);
    plat_printf("\n");
    dbg_dump_ip_buf(LOG_IP, "    dest ip:", ip_hdr->src_ip);
    plat_printf("\n");
    plat_printf("--------------- ip end ------------------ \n");

}
#else
#define display_ip_packet(pkt)
#define display_ip_frags()
#define rt_nlist_display()
#endif

/**
 * free buffer list in a fragment packet
 */
static void frag_free_buf_list (ip_frag_t * frag) {
    list_node_t* node;
    while ((node = list_remove_first(&frag->buf_list))) {
        packet_t* buf = list_entry(node, packet_t, node);
        packet_free(buf);
    }
}


/**
 * free a fragment packet
 */
static void frag_free (ip_frag_t * frag) {
    frag_free_buf_list(frag);
    list_remove(&frag_list, &frag->node);
    memory_pool_free(&frag_pool, frag);
}


/**
 * allocate a fragment packet, when there is no free one, reuse the oldest one
 */
static ip_frag_t * frag_alloc(void) {
    ip_frag_t * frag = memory_pool_alloc(&frag_pool, -1);
    if (!frag) {
        list_node_t* node = list_remove_last(&frag_list);
        frag = list_entry(node, ip_frag_t, node);
        if (frag) {
            frag_free_buf_list(frag);
        }
    }
    return frag;
}

static ip_frag_t* frag_find(ipaddr_t* ip, uint16_t id) {
    list_node_t* curr;
    list_for_each(curr, &frag_list) {
        ip_frag_t* frag = list_entry(curr, ip_frag_t, node);
        if (ipaddr_is_equal(ip, &frag->ip) && (id == frag->id)) {
            // move the head, take advantage of the time locality
            list_remove(&frag_list, curr);
            list_insert_first(&frag_list, curr);
            return frag;
        }
    }
    return (ip_frag_t*)0;
}

static void frag_add (ip_frag_t * frag, ipaddr_t* ip, uint16_t id) {
    ipaddr_copy(&frag->ip, ip);
    frag->tmo = 0;
    frag->id = id;
    list_node_init(&frag->node);
    init_list(&frag->buf_list);
    list_insert_first(&frag_list, &frag->node);
}


static net_err_t frag_insert(ip_frag_t * frag, packet_t * buf, ipv4_pkt_t* pkt) {
    if (list_count(&frag->buf_list) >= IP_FRAG_MAX_BUF_NR) {
        log_warning(LOG_IP, "too many buf on frag. drop it.\n");
        frag_free(frag);
        return NET_ERR_FULL;
    }

    // insert while maintaining the order of the buffer list
    list_node_t* node;
    list_for_each(node, &frag->buf_list) {
        packet_t* curr_buf = list_entry(node, packet_t, node);
        ipv4_pkt_t* curr_pkt = (ipv4_pkt_t*)packet_data(curr_buf);
        uint16_t curr_start = get_frag_start(curr_pkt);
        if (get_frag_start(pkt) == curr_start) {
            // overlap, drop it
            return NET_ERR_EXIST;
        } else if (get_frag_end(pkt) <= curr_start) {
            // insert before
            list_node_t* pre = list_node_pre(node);
            if (pre) {
                list_insert_after(&frag->buf_list, pre, &buf->node);
            } else {
                list_insert_first(&frag->buf_list, &buf->node);
            }
            return NET_OK;
        }
    }
    list_insert_last(&frag->buf_list, &buf->node);
    return NET_OK;
}


static int frag_is_all_arrived(ip_frag_t* frag) {
    int offset = 0;
    ipv4_pkt_t* pkt = (ipv4_pkt_t*)0;
    list_node_t* node;
    // check the continuity of the fragments from head to tail
    list_for_each(node, &frag->buf_list) {
        packet_t * buf = list_entry(node, packet_t, node);
        pkt = (ipv4_pkt_t*)packet_data(buf);
        int curr_offset = get_frag_start(pkt);
        if (curr_offset != offset) {
            return 0;
        }
        offset += get_data_size(pkt);
    }
    // the more flag must be 0 for the last fragment
    return pkt ? !pkt->hdr.more : 0;
}


static packet_t * frag_join (ip_frag_t * frag) {
    packet_t * target = (packet_t *)0;
    // because the fragments are in order, we can just keep popping the first fragment
    // and join it with the previous one until we have all the fragments
    list_node_t *node;
    while ((node = list_remove_first(&frag->buf_list))) {
        packet_t * curr = list_entry(node, packet_t, node);
        if (!target) {
            target = curr;
            continue;
        }
        // we didn't remove the ip header when inserting fragments,
        // and we reuse the first packet buffer
        ipv4_pkt_t * pkt = (ipv4_pkt_t *)packet_data(curr);
        net_err_t err = packet_remove_header(curr, ipv4_hdr_size(pkt));
        if (err < 0) {
            log_error(LOG_IP,"remove header for failed, err = %d\n", err);
            // don't forget to free the curr, because we have already taken it off the list
            packet_free(curr);
            goto free_and_return;
        }
        // join two packets, the curr will be freed in packet_join()
        err = packet_join(target, curr);
        if (err < 0) {
            log_error(LOG_IP,"join ip frag failed. err = %d\n", err);
            packet_free(curr);
            goto free_and_return;
        }
    }
    frag_free(frag);
    return target;
    free_and_return:
    // free the target and fragmented packet
    // be careful when there is no fragments, the target might be null
    if (target) {
        packet_free(target);
    }
    frag_free(frag);
    return (packet_t *)0;
}


// timer for fragmentation timeout
static void frag_tmo(net_timer_t* timer, void * arg) {
    list_node_t* curr, * next;
    //log_info(LOG_IP, "scan frag");
    for (curr = list_first(&frag_list); curr; curr = next) {
        next = list_node_next(curr);
        ip_frag_t * frag = list_entry(curr, ip_frag_t, node);
        if (--frag->tmo <= 0) {
            frag_free(frag);
        }
    }
    //display_ip_frags();
}


static net_err_t frag_init(void) {
    init_list(&frag_list);
    memory_pool_init(&frag_pool, frag_array, sizeof(ip_frag_t), IP_FRAGS_MAX_NR, LOCKER_NONE);
    net_err_t err = net_timer_add(&frag_timer, "frag timer", frag_tmo, (void *)0,
                                  IP_FRAG_SCAN_PERIOD * 1000, NET_TIMER_RELOAD);
    if (err < 0) {
        log_error(LOG_IP, "create frag timer failed.\n");
        return err;
    }
    return NET_OK;
}

void rt_init(void) {
    init_list(&rt_list);
    memory_pool_init(&rt_mblock, rt_table, sizeof(rentry_t), IP_RTABLE_SIZE, LOCKER_NONE);
}


void rt_add(ipaddr_t * net, ipaddr_t* mask, ipaddr_t* next_hop, netif_t* netif) {
    rentry_t* entry = (rentry_t*)memory_pool_alloc(&rt_mblock, -1);
    if (!entry) {
        log_warning(LOG_IP, "alloc rt entry failed.");
        return;
    }
    ipaddr_copy(&entry->net, net);
    ipaddr_copy(&entry->mask, mask);
    ipaddr_copy(&entry->next_hop, next_hop);
    entry->mask_1_cnt = ipaddr_1_cnt(mask);
    entry->netif = netif;
    list_insert_last(&rt_list, &entry->node);
    rt_nlist_display();
}


void rt_remove(ipaddr_t * net, ipaddr_t * mask) {
    list_node_t * node;
    list_for_each(node, &rt_list) {
        rentry_t* entry = list_entry(node, rentry_t, node);
        if (ipaddr_is_equal(&entry->net, net) && ipaddr_is_equal(&entry->mask, mask)) {
            list_remove(&rt_list, node);
            log_info(LOG_IP, "remove a route info:");
            dbg_dump_ip(LOG_IP, "net:", net);
            dbg_dump_ip(LOG_IP, "mask:", mask);
            plat_printf("\n");
            return;
        }
    }
}


rentry_t* rt_find(ipaddr_t * ip) {
    rentry_t* e = (rentry_t*)0;
    list_node_t* node;
    list_for_each(node, &rt_list) {
        rentry_t* entry = list_entry(node, rentry_t, node);
        // ip & mask != entry->net就跳过
        ipaddr_t net = ipaddr_get_net(ip, &entry->mask);
        if (!ipaddr_is_equal(&net, &entry->net)) {
            continue;
        }
        // if the current matched entry has a longer mask, update it
        if (!e || (e->mask_1_cnt < entry->mask_1_cnt)) {
            e = entry;
        }
    }
    return e;
}



net_err_t ipv4_init(void) {
    log_info(LOG_IP,"init ip\n");
    net_err_t err = frag_init();
    if (err < 0) {
        log_error(LOG_IP,"failed. err = %d", err);
        return err;
    }
    rt_init();
    log_info(LOG_IP,"done.");
    return NET_OK;
}

static inline void set_header_size(ipv4_pkt_t* pkt, int size) {
    pkt->hdr.shdr = size / 4;
}

static void iphdr_ntohs(ipv4_pkt_t* pkt) {
    pkt->hdr.total_len = e_ntohs(pkt->hdr.total_len);
    pkt->hdr.id = e_ntohs(pkt->hdr.id);
    pkt->hdr.frag_all = e_ntohs(pkt->hdr.frag_all);
}

static void iphdr_htons(ipv4_pkt_t* pkt) {
    pkt->hdr.total_len = e_htons(pkt->hdr.total_len);
    pkt->hdr.id = e_htons(pkt->hdr.id);
    pkt->hdr.frag_all = e_htons(pkt->hdr.frag_all);
}

/**
 * validate size and checksum of ipv4 packet
 * */
static net_err_t validate_ipv4_pkt(ipv4_pkt_t* pkt, int size) {
    if (pkt->hdr.version != NET_VERSION_IPV4) {
        log_warning(LOG_IP, "invalid ip version, only support ipv4!\n");
        return NET_ERR_NOT_SUPPORT;
    }

    int hdr_len = ipv4_hdr_size(pkt);
    if (hdr_len < sizeof(ipv4_hdr_t)) {
        log_warning(LOG_IP, "IPv4 header error: %d!", hdr_len);
        return NET_ERR_SIZE;
    }
    int total_size = e_ntohs(pkt->hdr.total_len);
    if ((total_size < sizeof(ipv4_hdr_t)) || (size < total_size)) {
        log_warning(LOG_IP, "ip packet size error: %d!\n", total_size);
        return NET_ERR_SIZE;
    }
    if (pkt->hdr.hdr_checksum) {
        uint16_t c = checksum16(0,(uint16_t*)pkt, hdr_len, 0, 1);
        if (c != 0) {
            log_warning(LOG_IP, "Bad checksum: %0x(correct is: %0x)\n", pkt->hdr.hdr_checksum, c);
            return NET_ERR_BROKEN;
        }
    }
    return NET_OK;
}


/**
 * this function handles single normal ip packet, without fragmentation
 * Be careful: when called by ipv4_in, the endian of the packet header is already converted to host byte order
 * */
static net_err_t ip_normal_in(netif_t* netif, packet_t* packet, ipaddr_t* src, ipaddr_t * dest) {
    ipv4_pkt_t* pkt = (ipv4_pkt_t*)packet_data(packet);
    switch (pkt->hdr.protocol) {
        case NET_PROTOCOL_ICMPv4: {
            net_err_t err = icmpv4_in(src, &netif->ipaddr, packet);
            if (err < 0) {
                log_warning(LOG_IP, "icmp in failed.\n");
                return err;
            }
            return NET_OK;
        }
        case NET_PROTOCOL_UDP: {
            iphdr_htons(pkt);   // be careful
            icmpv4_out_unreach(src, &netif->ipaddr, ICMPv4_UNREACH_PORT, packet);
            break;
        }
        case NET_PROTOCOL_TCP:
            break;
        default:{
            //log_warning(LOG_IP, "unknown protocol %d, .\n", pkt->hdr.protocol);
            net_err_t err = raw_in(packet);
            if (err < 0) {
                log_warning(LOG_IP, "raw in error. err = %d\n", err);
            }
            return NET_ERR_UNREACH;
            break;
        }
    }
    return NET_ERR_NOT_SUPPORT;
}



static net_err_t ip_frag_in (netif_t * netif, packet_t * buf, ipaddr_t* src, ipaddr_t* dest) {
    ipv4_pkt_t * curr = (ipv4_pkt_t *)packet_data(buf);
    ip_frag_t * frag = frag_find(src, curr->hdr.id);
    if (!frag) {
        frag = frag_alloc();
        frag_add(frag, src, curr->hdr.id);
    }
        net_err_t err = frag_insert(frag, buf, curr);
        if (err < 0) {
            log_warning(LOG_IP, "frag insert failed.");
            return err;
        }
    if (frag_is_all_arrived(frag)) {
        packet_t * full_buf = frag_join(frag);
//        log_info(LOG_IP, "join all ip frags success.\n");
//        display_ip_frags();
        if (!full_buf) {
            log_error(LOG_IP,"join all ip frags failed.\n");
            display_ip_frags();
            return NET_OK;
        }
        err = ip_normal_in(netif, full_buf, src, dest);
        if (err < 0) {
            log_warning(LOG_IP,"ip frag in error. err=%d\n", err);
            // the full_buf has to be freed in this function
            packet_free(full_buf);
            return NET_OK;
        }
    }
    display_ip_frags();
    return NET_OK;
}


net_err_t ipv4_in(netif_t* netif, packet_t* buf) {
    log_info(LOG_IP, "IP in\n");
    net_err_t err = packet_set_cont(buf, sizeof(ipv4_hdr_t));
    if (err < 0) {
        log_error(LOG_IP, "adjust header failed. err=%d\n", err);
        return err;
    }

    ipv4_pkt_t* pkt = (ipv4_pkt_t*)packet_data(buf);
    if (validate_ipv4_pkt(pkt, buf->total_size) != NET_OK) {
        log_warning(LOG_IP, "packet is broken. drop it.\n");
        return err;
    }
    // convert the fields in header to host byte order
    iphdr_ntohs(pkt);

    // total size has to be greater than header size
    // and there might be padding zeros at tail, resize the packet to total size
    // total_size = ip-header + payload
    // ethernet-header + [ip-header + payload + padding zeros] + fcs
    err = packet_resize(buf, pkt->hdr.total_len);
    if (err < 0) {
        log_error(LOG_IP, "ip packet resize failed. err=%d\n", err);
        return err;
    }
    log_info(LOG_IP, "ip packet in. total_size=%d\n", buf->total_size);
    // here we don't forward the packet, we just handle packets that sent to us
    ipaddr_t dest_ip, src_ip;
    ipaddr_from_buf(&dest_ip, pkt->hdr.dest_ip);
    ipaddr_from_buf(&src_ip, pkt->hdr.src_ip);

    // check if the destination ip is us or broadcast
    if (!ipaddr_is_match(&dest_ip, &netif->ipaddr, &netif->netmask)) {
        return NET_ERR_UNREACH;
    }
    if (pkt->hdr.offset || pkt->hdr.more) {
        err = ip_frag_in(netif, buf, &src_ip, &dest_ip);
    } else {
        err = ip_normal_in(netif, buf, &src_ip, &dest_ip);
    }
    return err;
}

/**
 * in the ip header, dest_ip ius the parameter dest
 * and netif_out, we use next
 * */
static net_err_t ip_frag_out(uint8_t protocol, ipaddr_t* dest,
                             ipaddr_t* src, packet_t* buf,  ipaddr_t * next, netif_t * netif) {
    log_info(LOG_IP,"frag send an ip packet.\n");
    packet_reset_pos(buf);
    int offset = 0;
    int total = buf->total_size;        // this does not include the header
    while (total) {
        int curr_size = total;
        if (curr_size > netif->mtu - sizeof(ipv4_hdr_t)) {
            curr_size = netif->mtu - sizeof(ipv4_hdr_t);
        }

        // make the fragment offset 8 bytes aligned
        // this is because there are only 13 bits for offset field in header,
        // and the unit for this field is 8 bytes
        if (curr_size < total) {
            curr_size &= ~0x7;
        }

        packet_t * dest_buf = packet_alloc(curr_size + sizeof(ipv4_hdr_t));
        if (!dest_buf) {
            log_error(LOG_IP,"alloc buf for frag send failed.\n");
            return NET_ERR_MEM;
        }
        ipv4_pkt_t * pkt = (ipv4_pkt_t *)packet_data(dest_buf);
        pkt->hdr.shdr_all = 0;
        pkt->hdr.version = NET_VERSION_IPV4;
        set_header_size(pkt, sizeof(ipv4_hdr_t));
        pkt->hdr.total_len = dest_buf->total_size;
        pkt->hdr.id = packet_id;
        pkt->hdr.frag_all = 0;
        pkt->hdr.ttl = NET_IP_DEF_TTL;
        pkt->hdr.protocol = protocol;
        pkt->hdr.hdr_checksum = 0;
        if (!src || ipaddr_is_any(src)) {
            ipaddr_to_buf(&netif->ipaddr, pkt->hdr.src_ip);
        } else {
            ipaddr_to_buf(src, pkt->hdr.src_ip);
        }
        ipaddr_to_buf(dest, pkt->hdr.dest_ip);
        pkt->hdr.offset = offset >> 3;      // the unit is 8 bytes
        pkt->hdr.more = total > curr_size;

        // copy data from buf to dest_buf
        packet_seek(dest_buf, sizeof(ipv4_hdr_t));
        net_err_t err = packet_copy(dest_buf, buf, curr_size);
        if (err < 0) {
            log_error(LOG_IP,"frag copy failed. error = %d.\n", err);
            packet_free(dest_buf);
            return err;
        }
        packet_remove_header(buf, curr_size);
        packet_reset_pos(buf);
        iphdr_htons(pkt);
        // before calculate the checksum, reset the pos
        packet_seek(dest_buf, 0);
        pkt->hdr.hdr_checksum = packet_checksum16(dest_buf, ipv4_hdr_size(pkt), 0, 1);
        display_ip_packet((ipv4_pkt_t*)pkt);
        err = netif_out(netif, next, dest_buf);
        if (err < 0) {
            log_warning(LOG_IP,"ip send error. err = %d\n", err);
            packet_free(dest_buf);
            return err;
        }
        total -= curr_size;
        offset += curr_size;
    }
    packet_id++;
    packet_free(buf);
    return NET_OK;
}

/**
 * the src can be null, if it is null,
 * we use the ip address of the netif matched in the routing table as the source ip
 * */
net_err_t ipv4_out(uint8_t protocol, ipaddr_t* dest, ipaddr_t * src, packet_t* packet) {
    log_info(LOG_IP,"send an ip packet.\n");
    rentry_t* rt = rt_find(dest);
    if (rt == (rentry_t *)0) {
        log_error(LOG_IP,"send failed. no route.");
        return NET_ERR_UNREACH;
    }
    ipaddr_t next_hop;
    if (ipaddr_is_any(&rt->next_hop)) {
        // if the dest is in the LAN, that means we only need one direct hop, make the next hop the dest itself
        ipaddr_copy(&next_hop, dest);
    } else {
        // if the dest is not in the LAN, we need to use the next hop provided by the routing table
        ipaddr_copy(&next_hop, &rt->next_hop);
    }
    //netif_t * netif = netif_get_default();
    if (rt->netif->mtu && ((packet->total_size + sizeof(ipv4_hdr_t)) > rt->netif->mtu)) {
        net_err_t err = ip_frag_out(protocol, dest, src, packet, &next_hop, rt->netif);
        if (err < 0) {
            log_warning(LOG_IP, "send ip frag packet failed. error = %d\n", err);
            return err;
        }
        return NET_OK;
    }
    net_err_t err = packet_add_header(packet, sizeof(ipv4_hdr_t), 1);
    if (err < 0) {
        log_error(LOG_IP, "no enough space for ip header, curr size: %d\n", packet->total_size);
        return NET_ERR_SIZE;
    }
    ipv4_pkt_t * ip_datagram = (ipv4_pkt_t*)packet_data(packet);
    ip_datagram->hdr.shdr_all = 0;
    ip_datagram->hdr.version = NET_VERSION_IPV4;
    set_header_size(ip_datagram, sizeof(ipv4_hdr_t));
    ip_datagram->hdr.total_len = packet->total_size;
    ip_datagram->hdr.id = packet_id++;        // static variable
    ip_datagram->hdr.frag_all = 0;
    ip_datagram->hdr.ttl = NET_IP_DEF_TTL;
    ip_datagram->hdr.protocol = protocol;
    ip_datagram->hdr.hdr_checksum = 0;
    if (!src || ipaddr_is_any(src)) {
        ipaddr_to_buf(&rt->netif->ipaddr, ip_datagram->hdr.src_ip);
    } else {
        ipaddr_to_buf(src, ip_datagram->hdr.src_ip);
    }
    ipaddr_to_buf(dest, ip_datagram->hdr.dest_ip);
    display_ip_packet(ip_datagram);
    // convert the fields in header to network byte order
    iphdr_htons(ip_datagram);
    packet_reset_pos(packet);
    ip_datagram->hdr.hdr_checksum = packet_checksum16(packet, ipv4_hdr_size(ip_datagram), 0, 1);
    err = netif_out(rt->netif, &next_hop, packet);
    if (err < 0) {
        log_warning(LOG_IP, "send ip packet failed. error = %d\n", err);
        return err;
    }
    return NET_OK;
}
