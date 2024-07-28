#include <stdio.h>
#include <string.h>
#include "net_plat.h"
#include "net_api.h"

/**
 * TCP download test
 * */
void download_test (const char * filename, int port) {
    printf("try to download %s from %s: %d\n", filename, friend0_ip, port);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("create socket error\n");
        goto failed;
    }
    FILE * file = fopen(filename, "wb");
    if (file == (FILE *)0) {
        printf("open file failed.\n");
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(friend0_ip);
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("connect error\n");
        goto failed;
    }
    char buf[8192];
    ssize_t total_size = 0;
    int rcv_size;
    while ((rcv_size = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, rcv_size, file);
        printf(".");
        total_size += rcv_size;
    }
    if (rcv_size < 0) {
        printf("rcv file size: %d\n", (int)total_size);
        goto failed;
    }
    printf("rcv file size: %d\n", (int)total_size);
    printf("rcv file ok\n");
    // close(sockfd);
    closesocket(sockfd);
    fclose(file);
    return;

failed:
    printf("rcv file error\n");
    // close(sockfd);
    closesocket(sockfd);
    if (file) {
        fclose(file);
    }
    return;
}