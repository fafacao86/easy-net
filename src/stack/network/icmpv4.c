#include "icmpv4.h"
#include "log.h"
#include "ipv4.h"


static net_err_t validate_icmp(icmpv4_pkt_t * pkt, int size, packet_t * packet) {
    if (size <= sizeof(icmpv4_hdr_t)) {
        log_warning(LOG_ICMP, "size error: %d", size);
        return NET_ERR_SIZE;
    }
    uint16_t checksum = packet_checksum16(packet, size, 0, 1);
    if (checksum != 0) {
        log_warning(LOG_ICMP, "Bad checksum %0x(correct is: %0x)\n", pkt->hdr.checksum, checksum);
        return NET_ERR_BROKEN;
    }
    return NET_OK;
}

/**
 *  there is no size field in ICMP, so we need to derive it from IP header.
 * the packet in the parameter is ip packet, we need to remove the ip header ourselves.
 */
net_err_t icmpv4_in(ipaddr_t *src, ipaddr_t * netif_ip, packet_t *packet) {
    log_info(LOG_ICMP, "icmp in !\n");
    ipv4_pkt_t* ip_pkt = (ipv4_pkt_t*)packet_data(packet);
    int iphdr_size = ip_pkt->hdr.shdr * 4;
    net_err_t err = packet_set_cont(packet, sizeof(icmpv4_hdr_t) + iphdr_size);
    if (err < 0) {
        log_error(LOG_ICMP, "set icmp cont failed");
        return err;
    }
    ip_pkt = (ipv4_pkt_t*)packet_data(packet);

    err = packet_remove_header(packet, iphdr_size);
    if (err < 0) {
        log_error(LOG_IP, "remove ip header failed. err = %d\n", err);
        return NET_ERR_SIZE;
    }
    packet_reset_pos(packet);

    icmpv4_pkt_t * icmp_pkt = (icmpv4_pkt_t*)packet_data(packet);
    if ((err = validate_icmp(icmp_pkt, packet->total_size, packet)) != NET_OK) {
        log_warning(LOG_ICMP, "icmp pkt error.drop it. err=%d", err);
        return err;
    }
    return NET_OK;
}

net_err_t icmpv4_init(void) {
    log_info(LOG_ICMP, "init icmp");
    log_info(LOG_ICMP, "done");
    return NET_OK;
}
