#ifndef EASY_NET_ARP_H
#define EASY_NET_ARP_H
#include "packet_buffer.h"
#include "netif.h"
#include "ether.h"
#include "ipaddr.h"

#define ARP_HW_ETHER            0x1             // hardware address type: ethernet
#define ARP_REQUEST             0x1             // ARP REQUEST
#define ARP_REPLY               0x2             // ARP REPLY

/**
 * arp packet
 */
#pragma pack(1)
typedef struct _arp_pkt_t {
    uint16_t htype;         // hardware type, here we only support ethernet
    uint16_t ptype;         // protocol type, here we only support IPv4
    uint8_t hlen;           // hardware address length, MAC is 6 bytes
    uint8_t plen;           // protocol address length, IPv4 is 4 bytes
    uint16_t opcode;        // operation code
    uint8_t send_haddr[ETH_HWA_SIZE];
    uint8_t send_paddr[IPV4_ADDR_SIZE];
    uint8_t target_haddr[ETH_HWA_SIZE];
    uint8_t target_paddr[IPV4_ADDR_SIZE];
}arp_pkt_t;
#pragma pack()

/**
 * ARP cache entry
 */
typedef struct _arp_entry_t {
    uint8_t paddr[IPV4_ADDR_SIZE];      // protocol address, IPv4
    uint8_t haddr[ETH_HWA_SIZE];        // hardware address, MAC

    enum {
        NET_ARP_FREE = 0x1234,          // FREE
        NET_ARP_RESOLVED,               // RESOLVED
        NET_ARP_WAITING,                // PENDING
    } state;
    int tmo;                // timeout
    int retry;              // retry count
    netif_t* netif;
    list_node_t node;
    list_t buf_list;        // packets to be sent when resolved
}arp_entry_t;

net_err_t arp_init (void);
net_err_t arp_make_request(netif_t* netif, const ipaddr_t* pro_addr);
net_err_t arp_make_gratuitous(netif_t* netif);
net_err_t arp_in(netif_t* netif, packet_t * packet);
net_err_t arp_make_reply(netif_t* netif, packet_t* packet);
net_err_t arp_resolve(netif_t* netif, const ipaddr_t* ipaddr, packet_t* packet);
const uint8_t* arp_find(netif_t* netif, ipaddr_t* ip_addr);
void arp_clear(netif_t * netif);
#endif //EASY_NET_ARP_H
