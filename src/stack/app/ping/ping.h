#ifndef EASY_NET_PING_H
#define EASY_NET_PING_H

#include <stdint.h>
#if defined(SYS_PLAT_WINDOWS)
#include <winsock2.h>
#include <WS2tcpip.h>
#include <time.h>

#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#define PING_BUFFER_SIZE			4096
#define PING_DEFAULT_ID				0x200

#pragma pack(1)

/**
 * ip header for ping
 */
typedef struct _ip_hdr_t {
    uint8_t shdr : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    uint8_t	src_ip[4];
    uint8_t dest_ip[4];
}ip_hdr_t;


typedef struct _icmp_hdr_t {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;    // there might be multiple ping program running
    uint16_t seq;   // there might be multiple ongoing ping request in a single ping program
}icmp_hdr_t;


typedef struct _echo_req_t {
    icmp_hdr_t echo_hdr;
    clock_t time;
    char buf[PING_BUFFER_SIZE];
}echo_req_t;

typedef struct _echo_reply_t {
    ip_hdr_t iphdr;
    icmp_hdr_t echo_hdr;
    clock_t time;
    char buf[PING_BUFFER_SIZE];
}echo_reply_t;
#pragma pack()

typedef struct _ping_t {
    echo_req_t req;
    echo_reply_t reply;
}ping_t;

void ping_run(ping_t * ping, const char * dest, int count, int size, int interval);

#endif //EASY_NET_PING_H
