#include <string.h>
#include "sys_plat.h"
#include "tcp_echo_server.h"
#include "net_api.h"
#include "log.h"

void tcp_echo_server_start(int port) {
    plat_printf("tcp server start, port = %d\n", port);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int s2 = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        plat_printf("open socket error");
        return;
    }
    struct sockaddr_in server_addr;
    plat_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(s, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        plat_printf("connect error");
        goto end;
    }

    struct sockaddr_in server_addr2;
    plat_memset(&server_addr2, 0, sizeof(server_addr2));
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_addr.s_addr = INADDR_ANY;
    server_addr2.sin_port = htons(port+1);
    if (bind(s2, (const struct sockaddr*)&server_addr2, sizeof(server_addr2)) < 0) {
        plat_printf("connect error");
        goto end;
    }

    listen(s, 5);
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        log_info(LOG_TCP, "waiting for client connection");
        int client = accept(s,  (struct sockaddr*)&client_addr, &addr_len);
        log_info(LOG_TCP, "client connected");
        if (client < 0) {
            printf("accept error");
            break;
        }
        printf("tcp echo server:connect ip: %s, port: %d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        char buf[128];
        ssize_t size;
        while ((size = recv(client, buf, sizeof(buf), 0)) > 0) {
            printf("recv bytes: %d\n", (int)size);
            send(client, buf, size, 0);
        }
        close(client);
    }
end:
    close(s);
}

