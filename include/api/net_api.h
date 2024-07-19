#ifndef EASY_NET_NET_API_H
#define EASY_NET_NET_API_H

#include <stdint.h>
#include "socket.h"
char* x_inet_ntoa(struct x_in_addr in);
uint32_t x_inet_addr(const char* str);
int x_inet_pton(int family, const char *strptr, void *addrptr);
const char * x_inet_ntop(int family, const void *addrptr, char *strptr, size_t len);


// redefine some functions to avoid conflicts with platform
#define in_addr             x_in_addr
#define sockaddr_in         x_sockaddr_in

#undef htons
#define htons(v)                x_htons(v)

#undef ntohs
#define ntohs(v)                x_ntohs(v)

#undef htonl
#define htonl(v)                x_htonl(v)

#undef ntohl
#define ntohl(v)                x_ntohl(v)

#define inet_ntoa(addr)             x_inet_ntoa(addr)
#define inet_addr(str)              x_inet_addr(str)

#define inet_pton(family, strptr, addrptr)          x_inet_pton(family, strptr, addrptr)
#define inet_ntop(family, addrptr, strptr, len)     x_inet_ntop(family, addrptr, strptr, len)
#define socket(family, type, protocol)              x_socket(family, type, protocol)

#endif //EASY_NET_NET_API_H
