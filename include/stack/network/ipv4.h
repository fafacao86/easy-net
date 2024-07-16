#ifndef EASY_NET_IPV4_H
#define EASY_NET_IPV4_H

#include <stdint.h>
#include "net_errors.h"
#include "netif.h"

#define IPV4_ADDR_SIZE          4
#define NET_VERSION_IPV4        4

#pragma pack(1)

/**
 * header length is 20 bytes minimum, and can be up to 60 bytes with options.
 * checkout TCP-IP Illustrated, Volume 1, chapter 5
 */
typedef struct _ipv4_hdr_t {
    union {
        struct {
#if NET_ENDIAN_LITTLE
            uint16_t shdr : 4;           // header length, unit is 4 bytes
            uint16_t version : 4;
            uint16_t tos : 8;
#else
            uint16_t version : 4;
            uint16_t shdr : 4;
            uint16_t tos : 8;
#endif
        };
        uint16_t shdr_all;
    };          // first 16 bits of the header, it contains multiple small fields
    uint16_t total_len;
    uint16_t id;		        // for reassemble
    uint16_t frag_all;

    uint8_t ttl;                // time to live
    uint8_t protocol;	        // upper layer protocol number
    uint16_t hdr_checksum;
    uint8_t	src_ip[IPV4_ADDR_SIZE];
    uint8_t dest_ip[IPV4_ADDR_SIZE];
}ipv4_hdr_t;


typedef struct _ipv4_pkt_t {
    ipv4_hdr_t hdr;
    uint8_t data[1];
}ipv4_pkt_t;

#pragma pack()

net_err_t ipv4_init(void);
net_err_t ipv4_in(netif_t * netif, packet_t *buf);
static inline int ipv4_hdr_size(ipv4_pkt_t* pkt) {
    return pkt->hdr.shdr * 4;
}
#endif //EASY_NET_IPV4_H
