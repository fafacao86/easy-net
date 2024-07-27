#include "tcp_echo_client.h"

#include <string.h>
#include "tcp_echo_client.h"
#include "sys_plat.h"
#include "net_api.h"


int tcp_echo_client_start (const char* ip, int port) {
    plat_printf("tcp echo client, ip: %s, port: %d\n", ip, port);
    plat_printf("Enter quit to exit\n");
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        plat_printf("tcp echo client: open socket error");
        goto end;
    }

    struct sockaddr_in server_addr;
    plat_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    if (connect(s, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        plat_printf("connect error");
        goto end;
    }
    char sbuf[2048];
    for (int i = 0; i < sizeof(sbuf); i++) {
        sbuf[i] = 'a' + i % 26;
    }
    //fgets(sbuf, sizeof(sbuf), stdin);
    for (int i = 0; i < 10; i++) {
        //for (int i = 0; i < 100000; i++) {
        ssize_t size = send(s, sbuf, sizeof(sbuf), 0);
        if (size < 0) {
            printf("send error: size=%d\n", (int)size);
            break;
        }
        //printf("send ok: %i\n", i);
        //sys_sleep(10);
    }
    fgets(sbuf, sizeof(sbuf), stdin);
    close(s);
    return 0;
end:
    if (s >= 0) {
        close(s);
    }
    return -1;
}

