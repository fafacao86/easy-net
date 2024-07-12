#include <string.h>
#include "netif.h"
#include "ether.h"
#include "log.h"
#include "utils.h"
#include "protocols.h"

#if LOG_DISP_ENABLED(LOG_ETHER)
static void display_ether_display(char * title, ether_frame_t * frame, int size) {
    ether_hdr_t * hdr = (ether_hdr_t *)frame;

    plat_printf("\n--------------- %s ------------------ \n", title);
    plat_printf("\tlen: %d bytes\n", size);
    dump_mac("\tdest:", hdr->dest);
    dump_mac("\tsrc:", hdr->src);
    plat_printf("\ttype: %04x - ", e_ntohs(hdr->protocol));
    switch (e_ntohs(hdr->protocol)) {
    case NET_PROTOCOL_ARP:
        plat_printf("ARP\n");
        break;
    case NET_PROTOCOL_IPv4:
        plat_printf("IP\n");
        break;
    default:
        plat_printf("Unknown\n");
        break;
    }
    plat_printf("\n");
}

#else
#define display_ether_display(title, pkt, size)
#endif

static net_err_t ether_open(netif_t* netif) {
    return NET_OK;
}

static void ether_close(netif_t* netif) {

}

static net_err_t validate_frame_format(ether_frame_t * frame, int total_size) {
    if (total_size > (sizeof(ether_hdr_t) + ETHER_MTU)) {
        log_warning(LOG_ETHER, "frame size too big: %d", total_size);
        return NET_ERR_SIZE;
    }
    // the specification requires that the frame size is at least 64 bytes
    // but when using pcap, the padding might be stripped
    if (total_size < (sizeof(ether_hdr_t))) {
        log_warning(LOG_ETHER, "frame size too small: %d", total_size);
        return NET_ERR_SIZE;
    }
    return NET_OK;
}

static net_err_t ether_in(netif_t* netif, packet_t* packet) {
    log_info(LOG_ETHER, "ether in:");
    packet_set_cont(packet, sizeof(ether_hdr_t));

    ether_frame_t* pkt = (ether_frame_t*)packet_data(packet);
    net_err_t err;
    if ((err = validate_frame_format(pkt, packet->total_size)) != NET_OK) {
        log_error(LOG_ETHER, "ether frame error");
        return err;
    }

    display_ether_display("ether in", pkt, packet->total_size);
    packet_free(packet);
    return NET_OK;
}

/**
 * register ethernet link layer interface
 * */
net_err_t ether_init(void) {
    static const link_layer_t link_layer = {
            .type = NETIF_TYPE_ETHER,
            .open = ether_open,
            .close = ether_close,
            .in = ether_in,
            // .out = ether_out,
    };

    log_info(LOG_ETHER, "init ether");
    // register ethernet link layer interface
    net_err_t err = netif_register_layer(NETIF_TYPE_ETHER, &link_layer);
    if (err < 0) {
        log_info(LOG_ETHER, "error = %d", err);
        return err;
    }

    log_info(LOG_ETHER, "done.");
    return NET_OK;
}

const uint8_t * ether_broadcast_addr(void) {
    static const uint8_t broadcast_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return broadcast_addr;
}


/**
 * Send ethernet frame without ARP address resolution.
 * */
net_err_t ether_raw_out(netif_t* netif, uint16_t protocol, const uint8_t* dest, packet_t* packet) {
    net_err_t err;
    int size = packet_total_size(packet);
    if (size < ETHER_MIN_PAYLOAD) {
        log_info(LOG_ETHER, "resize from %d to %d", size, (int)ETHER_MIN_PAYLOAD);
        err = packet_resize(packet, ETHER_MIN_PAYLOAD);
        if (err < 0) {
            log_error(LOG_ETHER, "resize failed: %d", err);
            return err;
        }
        packet_reset_pos(packet);
        packet_seek(packet, size);
        packet_fill(packet, 0, ETHER_MIN_PAYLOAD - size);
    }

    err = packet_add_header(packet, sizeof(ether_hdr_t), 1);
    if (err < 0) {
        log_error(LOG_ETHER, "add header failed: %d", err);
        return NET_ERR_SIZE;
    }

    ether_frame_t * pkt = (ether_frame_t*)packet_data(packet);
    plat_memcpy(pkt->hdr.dest, dest, ETH_HWA_SIZE);
    plat_memcpy(pkt->hdr.src, netif->hwaddr.addr, ETH_HWA_SIZE);
    pkt->hdr.protocol = e_htons(protocol);

    display_ether_display("ether out", pkt, size);

    // if destination address is ourselves, put it in our input queue
    // instead of sending it out to the network, just like loop back
    if (plat_memcmp(netif->hwaddr.addr, dest, ETH_HWA_SIZE) == 0) {
        return netif_put_in(netif, packet, -1);
    } else {
        err = netif_put_out(netif, packet, -1);
        if (err < 0) {
            log_warning(LOG_ETHER, "put pkt out failed: %d", err);
            return err;
        }
        return netif->ops->transmit(netif);
    }
}