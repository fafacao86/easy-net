#ifndef EASY_NET_UDP_H
#define EASY_NET_UDP_H
#include "sock.h"
#include "socket.h"


#pragma pack(1)
typedef struct _udp_hdr_t {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t total_len;    // including header and data
    uint16_t checksum;
}udp_hdr_t;

typedef struct _udp_pkt_t {
    udp_hdr_t hdr;
    uint8_t data[1];
}udp_pkt_t;
#pragma pack()


typedef struct _udp_t {
    sock_t  base;                   // base class
    list_t recv_list;
    sock_wait_t rcv_wait;
}udp_t;

net_err_t udp_init(void);
sock_t* udp_create(int family, int protocol);
net_err_t udp_out(ipaddr_t* dest, uint16_t dport, ipaddr_t* src, uint16_t sport, packet_t* buf);
net_err_t udp_sendto (struct _sock_t * sock, const void* buf, size_t len, int flags, const struct x_sockaddr* dest,
                      x_socklen_t dest_len, ssize_t * result_len);
#endif //EASY_NET_UDP_H
