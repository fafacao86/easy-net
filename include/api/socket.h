#ifndef EASY_NET_SOCKET_H
#define EASY_NET_SOCKET_H

#include <stdint.h>
#include "ipaddr.h"
#include "sock.h"
/**
 * Linux Manual Page is a good reference for this part.
 *
 * this file should always be included at last, because it redefines some macros
 * */
int x_socket(int family, int type, int protocol);
ssize_t x_sendto(int sid, const void* buf, size_t len, int flags, const struct x_sockaddr* dest, x_socklen_t dest_len);
ssize_t x_recvfrom(int sid, void* buf, size_t len, int flags, struct x_sockaddr* src, x_socklen_t* src_len);
int x_setsockopt(int sockfd, int level, int optname, const char * optval, int optlen);
int x_close(int sockfd);
int x_connect(int sid, const struct x_sockaddr* addr, x_socklen_t len);
ssize_t x_send(int fd, const void* buf, size_t len, int flags);
ssize_t x_recv(int fd, void* buf, size_t len, int flags);
int x_bind(int sid, const struct x_sockaddr* addr, x_socklen_t len);


#undef AF_INET
#define AF_INET                 0               // IPv4

#undef SOCK_RAW
#define SOCK_RAW                1               // raw IP packet socket, default type is ICMP
#undef SOCK_DGRAM
#define SOCK_DGRAM              2               // datagram socket, default type is UDP


#undef IPPROTO_ICMP
#define IPPROTO_ICMP            1               // ICMP

#undef INADDR_ANY
#define INADDR_ANY              0               // all zeros for IPv4 address
// sockopt level
#undef SOL_SOCKET
#define SOL_SOCKET              0

// sockopt name
#undef SO_RCVTIMEO
#define SO_RCVTIMEO             1            // ms
#undef SO_SNDTIMEO
#define SO_SNDTIMEO             2            // ms


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

struct x_timeval {
    int tv_sec;             // seconds
    int tv_usec;            // microseconds
};


typedef struct _socket_t {
    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED,
    }state;
    sock_t * sock;
}x_socket_t;

#endif //EASY_NET_SOCKET_H
