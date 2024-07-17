#ifndef EASY_NET_ICMPV4_H
#define EASY_NET_ICMPV4_H

#include "net_errors.h"
#include "ipaddr.h"
#include "packet_buffer.h"
#pragma pack(1)
typedef struct _icmpv4_hdr_t {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
}icmpv4_hdr_t;


typedef struct _icmpv4_pkt_t {
    icmpv4_hdr_t hdr;
    union {
        uint32_t reverse;       // reserved 4 bytes
    };
    uint8_t data[1];
}icmpv4_pkt_t;
#pragma pack()
net_err_t icmpv4_init(void);
net_err_t icmpv4_in(ipaddr_t *src_ip, ipaddr_t* netif_ip, packet_t *packet);
#endif //EASY_NET_ICMPV4_H
