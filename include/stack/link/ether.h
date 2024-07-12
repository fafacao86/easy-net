#ifndef EASY_NET_ETHER_H
#define EASY_NET_ETHER_H
#include "net_errors.h"
/**
 * Ethernet II frame:
 *   |- Destination MAC address (6 bytes)
 *   |- Source MAC address (6 bytes)
 *   |- EtherType (2 bytes)   0x0800 for IPv4, 0x0806 for ARP
 *   |- Data (46-1500 bytes)
 *   |- checksum (4 bytes) this will be added by the hardware
 * */

#pragma pack(1)     // prevent alignment
typedef struct _ether_hdr_t {
    uint8_t dest[ETH_HWA_SIZE];
    uint8_t src[ETH_HWA_SIZE];
    uint16_t protocol;
}ether_hdr_t;

typedef struct _ether_pkt_t {
    ether_hdr_t hdr;                    // header
    uint8_t data[ETHER_MTU];              // payload
}ether_frame_t;
#pragma pack()

net_err_t ether_init(void);
net_err_t ether_in(netif_t* netif, packet_t* packet);
const uint8_t* ether_broadcast_addr(void);
net_err_t ether_raw_out(netif_t* netif, uint16_t protocol, const uint8_t* dest, packet_t* packet);

#endif //EASY_NET_ETHER_H
