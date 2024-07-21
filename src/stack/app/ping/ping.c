#include <string.h>
#include "ping.h"
#include "sys_plat.h"
#include "net_api.h"


static uint16_t checksum(void* buf, uint16_t len) {
    uint16_t* curr_buf = (uint16_t*)buf;
    uint32_t checksum = 0;

    while (len > 1) {
        checksum += *curr_buf++;
        len -= 2;
    }

    if (len > 0) {
        checksum += *(uint8_t*)curr_buf;
    }
    uint16_t high;
    while ((high = checksum >> 16) != 0) {
        checksum = high + (checksum & 0xffff);
    }

    return (uint16_t)~checksum;
}

/**
 * simple ping request
 */
void ping_run(ping_t * ping, const char* dest, int count, int size, int interval) {
    static uint16_t start_id = PING_DEFAULT_ID;
    char buf[512];

//#if defined(SYS_PLAT_WINDOWS)
//    SOCKET sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
//#else
    int sk = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
//#endif
    if (sk < 0) {
        printf("create socket error\n");
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = inet_addr(dest);;

    inet_ntop(AF_INET, &addr.sin_addr.s_addr, buf, sizeof(buf));
    printf("try to ping %s [%s]\n", dest, buf);

//#define USE_CONNECT
#ifdef USE_CONNECT
    connect(sk, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in));
#endif

//#if defined(SYS_PLAT_WINDOWS)
//    int tmo = 3000;
//#else
    struct timeval tmo;
    tmo.tv_sec = 5;
    tmo.tv_usec = 0;
//#endif
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof(tmo));

    // Unix Network Programming, put time in request packet data section
    size -= sizeof(clock_t);
    int fill_size = size > PING_BUFFER_SIZE ? PING_BUFFER_SIZE : size;
    for (int i = 0; i < fill_size; i++) {
        ping->req.buf[i] = i;
    }
    int total_size = sizeof(icmp_hdr_t) + sizeof(clock_t) + fill_size;
    for (int i = 0, seq = 0; i < count; i++, seq++) {
        ping->req.echo_hdr.id = start_id;
        ping->req.echo_hdr.seq = seq;
        ping->req.echo_hdr.type = 8;    // echo
        ping->req.echo_hdr.code = 0;
        ping->req.time = clock();        // current time before send
        ping->req.echo_hdr.checksum = 0;
        ping->req.echo_hdr.checksum = checksum(&ping->req, total_size);

#ifdef USE_CONNECT
        ssize_t size = send(sk, (const char*)&ping->req, total_size, 0);
#else
        ssize_t size = sendto(sk, (const char *)&ping->req, total_size, 0,
                        (struct sockaddr *)&addr, sizeof(addr));
        sys_sleep(1000);
#endif
        if (size < 0) {
            printf("send ping request failed.\n");
            break;
        }
        do {
            memset(&ping->reply, 0, sizeof(ping->reply));
#ifdef USE_CONNECT
            size = recv(sk, (char*)&ping->reply, sizeof(ping->reply), 0);
#else
            int addr_len = sizeof(addr);
            size = recvfrom(sk, (char*)&ping->reply, sizeof(ping->reply), 0,
                        (struct sockaddr*)&addr, &addr_len);
#endif
            if (size < 0) {
                printf("ping recv tmo.\n");
                break;
            }

            // check identity and sequence number
            if ((ping->req.echo_hdr.id == ping->reply.echo_hdr.id) &&
                (ping->req.echo_hdr.seq == ping->reply.echo_hdr.seq)) {
                break;
            }
        } while (1);

        if (size > 0) {
            int recv_size = size - sizeof(ip_hdr_t) - sizeof(icmp_hdr_t);
            if (memcmp(ping->req.buf, ping->reply.buf, recv_size - sizeof(clock_t))) {
                printf("recv data error\n");
                continue;
            }

            ip_hdr_t* iphdr = &ping->reply.iphdr;
            int send_size = fill_size + sizeof(clock_t);
            if (recv_size == send_size) {
                printf("Reply from %s: bytes = %d", inet_ntoa(addr.sin_addr), send_size);
            } else {
                printf("Reply from %s: bytes = %d(send = %d)", inet_ntoa(addr.sin_addr), recv_size, send_size);
            }

            int diff_ms = (clock() - ping->req.time) / (CLOCKS_PER_SEC / 1000);
            if (diff_ms < 1) {
                printf(" time<1ms, TTL=%d\n", iphdr->ttl);
            } else {
                printf(" time=%dms, TTL=%d\n", diff_ms, iphdr->ttl);
            }
            sys_sleep(interval);
        }
    }

#if defined(SYS_PLAT_WINDOWS)
    closesocket(sk);
#else
    close(sk);
#endif
}