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
        uint32_t reverse;       // reserved, in ping, we use this to store sequence number and id
    };
    uint8_t data[1];
}icmpv4_pkt_t;
#pragma pack()

typedef enum _icmp_type_t {
    ICMPv4_ECHO_REPLY = 0,                  // reply to a ping
    ICMPv4_ECHO_REQUEST = 8,                // request to ping
    ICMPv4_UNREACH = 3,                     // destination unreachable
}icmp_type_t;

typedef enum _icmp_code_t {
    ICMPv4_ECHO = 0,                        // ping uses echo
    ICMPv4_UNREACH_PROTO = 2,                 // protocol unreachable
    ICMPv4_UNREACH_PORT = 3,                 // port unreachable
}icmp_code_t;


net_err_t icmpv4_init(void);
net_err_t icmpv4_in(ipaddr_t *src_ip, ipaddr_t* netif_ip, packet_t *packet);
net_err_t icmpv4_out_unreach(ipaddr_t* dest_addr, ipaddr_t* src, uint8_t code, packet_t* ip_buf);
#endif //EASY_NET_ICMPV4_H
