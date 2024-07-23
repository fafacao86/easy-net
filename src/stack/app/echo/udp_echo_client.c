#include <string.h>
#include <stdio.h>
#if defined(SYS_PLAT_WINDOWS)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "sys_plat.h"
#include "net_api.h"

int udp_echo_client_start(const char* ip, int port) {
    printf("udp echo client, ip: %s, port: %d\n", ip, port);
    printf("Enter quit to exit\n");

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        printf("open socket error");
        goto end;
    }

    // be careful about the endianness of the ip address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

//#define USE_CONNECT
//#ifdef USE_CONNECT
    connect(s, (const struct x_sockaddr*)&server_addr, sizeof(server_addr));
//#endif
    printf(">>");
    char buf[128];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (strncmp(buf, "quit", 4) == 0) {
            break;
        }
        size_t total_len = strlen(buf);
#ifdef USE_CONNECT
        ssize_t size = x_send(s, buf, total_len, 0);
#else
        ssize_t size = sendto(s, buf, total_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
#endif
        if (size < 0) {
            printf("send error");
            goto end;
        }
        memset(buf, 0, sizeof(buf));
#ifdef USE_CONNECT
        size = recv(s, buf, sizeof(buf), 0);
#else
        struct sockaddr_in remote_addr;
        //socklen_t addr_len = sizeof(remote_addr);
        int addr_len = sizeof(remote_addr);
        size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&remote_addr, &addr_len);
#endif
        if (size < 0) {
            printf("recv error");
            goto end;
        }
        buf[sizeof(buf) - 1] = '\0';        // ensure null termination

        printf("%s", buf);
        printf("\n>>");
    }
    end:
    if (s >= 0) {
        close(s);
    }
    return -1;
}

