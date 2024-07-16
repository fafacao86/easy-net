#include "net_errors.h"
#include "ipv4.h"
#include "log.h"
#include "utils.h"

net_err_t ipv4_init(void) {
    log_info(LOG_IP,"init ip\n");

    log_info(LOG_IP,"done.");
    return NET_OK;
}


static void iphdr_ntohs(ipv4_pkt_t* pkt) {
    pkt->hdr.total_len = e_ntohs(pkt->hdr.total_len);
    pkt->hdr.id = e_ntohs(pkt->hdr.id);
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
            return 0;
        }
    }
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
    packet_free(buf);
    return NET_OK;
}
