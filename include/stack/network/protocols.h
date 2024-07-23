#ifndef EASY_NET_PROTOCOLS_H
#define EASY_NET_PROTOCOLS_H
typedef enum _protocol_t {
    NET_PROTOCOL_ARP = 0x0806,     // ARP
    NET_PROTOCOL_IPv4 = 0x0800,      // IPv4
    NET_PROTOCOL_ICMPv4 = 0x1,         // ICMP
    NET_PROTOCOL_UDP = 0x11,          // UDP
    NET_PROTOCOL_TCP = 0x06,          // TCP
    }protocol_t;

/**
 * reserved port numbers for common services
 * */
typedef enum _port_t {
    NET_PORT_EMPTY = 0,
}port_t;

#endif //EASY_NET_PROTOCOLS_H
