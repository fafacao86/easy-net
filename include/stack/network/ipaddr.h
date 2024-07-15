#ifndef EASY_NET_IPADDR_H
#define EASY_NET_IPADDR_H
#include <stdint.h>
#include "net_errors.h"

#define IPV4_ADDR_SIZE             4
#define IPV4_ADDR_BROADCAST       0xFFFFFFFF

/**
 * IP address
 */
typedef struct _ipaddr_t {
    enum {
        IPADDR_V4,
        IPADDR_V6,      // ipv6 for future use
    }type;

    union {
        // IP address is stored in big-endian format, which is also called network byte order.
        // checkout RFC 1700
        uint32_t q_addr;
        uint8_t a_addr[IPV4_ADDR_SIZE];
    };
}ipaddr_t;
void ipaddr_set_any(ipaddr_t * ip);
net_err_t ipaddr_from_str(ipaddr_t * dest, const char* str);
ipaddr_t * ipaddr_get_any(void);
void ipaddr_copy(ipaddr_t * dest, const ipaddr_t * src);
int ipaddr_is_equal(const ipaddr_t * ipaddr1, const ipaddr_t * ipaddr2);
void ipaddr_to_buf(const ipaddr_t* src, uint8_t* ip_buf);
void ipaddr_from_buf(ipaddr_t* dest, const uint8_t * ip_buf);
int ipaddr_is_local_broadcast(const ipaddr_t * ipaddr);
int ipaddr_is_direct_broadcast(const ipaddr_t * ipaddr, const ipaddr_t * netmask);
#endif //EASY_NET_IPADDR_H
