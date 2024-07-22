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
#define sockaddr            x_sockaddr
#define timeval         x_timeval

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
#define sendto(s, buf, len, flags, dest, dlen)      x_sendto(s, buf, len, flags, dest, dlen)
#define recvfrom(s, buf, len, flags, src, slen)     x_recvfrom(s, buf, len, flags, src, slen)
#define setsockopt(s, level, optname, optval, len)  x_setsockopt(s, level, optname, optval, len)
#define close(s)                                    x_close(s)

#endif //EASY_NET_NET_API_H
