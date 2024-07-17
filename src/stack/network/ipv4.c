#include "net_errors.h"
#include "ipv4.h"
#include "log.h"
#include "utils.h"
#include "protocols.h"
#include "icmpv4.h"

static uint16_t packet_id = 0;                  // incremental id for ipv4 packet


#if LOG_DISP_ENABLED(LOG_IP)
static void display_ip_packet(ipv4_pkt_t* pkt) {
    ipv4_hdr_t* ip_hdr = (ipv4_hdr_t*)&pkt->hdr;

    plat_printf("--------------- ip ------------------ \n");
    plat_printf("    Version:%d\n", ip_hdr->version);
    plat_printf("    Header len:%d bytes\n", ipv4_hdr_size(pkt));
    plat_printf("    Totoal len: %d bytes\n", ip_hdr->total_len);
    plat_printf("    Id:%d\n", ip_hdr->id);
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
#endif



net_err_t ipv4_init(void) {
    log_info(LOG_IP,"init ip\n");

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
    pkt->hdr.frag_all = e_ntohs(pkt->hdr.frag_all);
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
        uint16_t c = checksum16((uint16_t*)pkt, hdr_len, 0, 1);
        if (c != 0) {
            log_warning(LOG_IP, "Bad checksum: %0x(correct is: %0x)\n", pkt->hdr.hdr_checksum, c);
            return NET_ERR_BROKEN;
        }
    }
    return NET_OK;
}


/**
 * this function handles single normal ip packet, without fragmentation
 * */
static net_err_t ip_normal_in(netif_t* netif, packet_t* packet, ipaddr_t* src, ipaddr_t * dest) {
    ipv4_pkt_t* pkt = (ipv4_pkt_t*)packet_data(packet);
    display_ip_packet(pkt);
    switch (pkt->hdr.protocol) {
        case NET_PROTOCOL_ICMPv4: {
            net_err_t err = icmpv4_in(src, &netif->ipaddr, packet);
            if (err < 0) {
                log_warning(LOG_IP, "icmp in failed.\n");
                return err;
            }
            return NET_OK;
        }
        case NET_PROTOCOL_UDP:
            break;
        case NET_PROTOCOL_TCP:
            break;
        default:
            log_warning(LOG_IP, "unknown protocol %d, drop it.\n", pkt->hdr.protocol);
            break;
    }
    return NET_ERR_NOT_SUPPORT;
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
    err = ip_normal_in(netif, buf, &src_ip, &dest_ip);
    return err;
}


net_err_t ipv4_out(uint8_t protocol, ipaddr_t* dest, ipaddr_t * src, packet_t* packet) {
    log_info(LOG_IP,"send an ip packet.\n");

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
    ipaddr_to_buf(src, ip_datagram->hdr.src_ip);
    ipaddr_to_buf(dest, ip_datagram->hdr.dest_ip);

    display_ip_packet(ip_datagram);
    // convert the fields in header to network byte order
    iphdr_htons(ip_datagram);
    packet_reset_pos(packet);
    ip_datagram->hdr.hdr_checksum = packet_checksum16(packet, ipv4_hdr_size(ip_datagram), 0, 1);
    err = netif_out(netif_get_default(), dest, packet);
    if (err < 0) {
        log_warning(LOG_IP, "send ip packet failed. error = %d\n", err);
        return err;
    }
    return NET_OK;
}
