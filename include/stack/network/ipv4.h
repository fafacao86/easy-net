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
    union {
        struct {
#if NET_ENDIAN_LITTLE
            uint16_t offset : 13;               // fragment offset, unit is 8 bytes
            uint16_t more : 1;                  // this is not the last fragment, there are more fragments
            uint16_t disable : 1;               // 1-disable fragmentation，0-allow fragmentation
            uint16_t resvered : 1;              // reserved，must be 0
#else
            uint16_t resvered : 1;              // 保留，必须为0
            uint16_t disable : 1;               // 1-不允许分片，0-可以分片
            uint16_t more : 1;                  // 不是最后一个包，还有后续
            uint16_t offset : 13;               // 数据报分片偏移, 以8字节为单位，从0开始算
#endif
        };
        uint16_t frag_all;
    };

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

/**
 * one ip_frag_t represents one big packet that is fragmented into smaller packets.
 * the ip packet is identified by its source ip address and packet id.
 * buf_list is a list of fragments, each fragment is a packet_t.
 * */
typedef struct _ip_frag_t {
    ipaddr_t ip;                // source ip address of the fragmented packet
    uint16_t id;                // packet id
    int tmo;                    // timeout for reassembly
    list_t buf_list;            // list of fragments
    list_node_t node;
}ip_frag_t;


/**
 * routing table entry
 * */
typedef struct _rentry_t {
    ipaddr_t net;
    ipaddr_t mask;
    int mask_1_cnt;         // count of 1s in the mask, used for longest prefix match
    ipaddr_t next_hop;       // ip address of the next hop
    netif_t* netif;         // network interface to reach the next hop
    list_node_t node;
}rentry_t;

void rt_init(void);
void rt_add(ipaddr_t* net, ipaddr_t* mask, ipaddr_t* next_hop, netif_t* netif);
void rt_remove(ipaddr_t* net, ipaddr_t* mask);
rentry_t* rt_find(ipaddr_t * ip);

net_err_t ipv4_init(void);
net_err_t ipv4_in(netif_t * netif, packet_t *buf);
static inline int ipv4_hdr_size(ipv4_pkt_t* pkt) {
    return pkt->hdr.shdr * 4;
}
net_err_t ipv4_out(uint8_t protocol, ipaddr_t* dest, ipaddr_t* src, packet_t* packet);
#endif //EASY_NET_IPV4_H
