#ifndef EASY_NET_SOCKET_H
#define EASY_NET_SOCKET_H

#include <stdint.h>
#include "ipaddr.h"
/**
 * Linux Manual Page is a good reference for this part.
 *
 * this file should always be included at last, because it redefines some macros
 * */
int x_socket(int family, int type, int protocol);

#undef AF_INET
#define AF_INET                 0               // IPv4

#undef SOCK_RAW
#define SOCK_RAW                1

#undef IPPROTO_ICMP
#define IPPROTO_ICMP            1               // ICMP

#undef INADDR_ANY
#define INADDR_ANY              0               // all zeros for IPv4 address


#pragma pack(1)
/**
 * ipv4 address
 */
struct x_in_addr {
    union {
        struct {
            uint8_t addr0;
            uint8_t addr1;
            uint8_t addr2;
            uint8_t addr3;
        };

        uint8_t addr_array[IPV4_ADDR_SIZE];

#undef s_addr       // avoid conflict with macro in winsock
        uint32_t s_addr;
    };
};


/**
 * general socket address structure.
 * you can cast it to struct x_sockaddr_in freely
 */
struct x_sockaddr {
    uint8_t sa_len;
    uint8_t sa_family;
    uint8_t sa_data[14];
};


/**
 * Internet socket address, includes port and IP address.
 * the size of this structure is the same with struct sockaddr
 */
struct x_sockaddr_in {
    uint8_t sin_len;                // structure length 16 bytes
    uint8_t sin_family;             // AF_INET, TCP/IP protocol family
    uint16_t sin_port;              // port number in network byte order
    struct x_in_addr sin_addr;      // ip address
    char sin_zero[8];               // padding to align to 16 bytes
};
#pragma pack()
typedef struct _socket_t {
    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED,
    }state;
}x_socket_t;

#endif //EASY_NET_SOCKET_H
