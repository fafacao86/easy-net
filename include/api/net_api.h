#ifndef EASY_NET_NET_API_H
#define EASY_NET_NET_API_H

#include <stdint.h>
#include "socket.h"
#include "utils.h"
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
#define htons(v)                e_htons(v)

#undef ntohs
#define ntohs(v)                e_ntohs(v)

#undef htonl
#define htonl(v)                e_htonl(v)

#undef ntohl
#define ntohl(v)                e_ntohl(v)

#define inet_ntoa(addr)             x_inet_ntoa(addr)
#define inet_addr(str)              x_inet_addr(str)

#define inet_pton(family, strptr, addrptr)          x_inet_pton(family, strptr, addrptr)
#define inet_ntop(family, addrptr, strptr, len)     x_inet_ntop(family, addrptr, strptr, len)
#define socket(family, type, protocol)              x_socket(family, type, protocol)
#define sendto(s, buf, len, flags, dest, dlen)      x_sendto(s, buf, len, flags, dest, dlen)
#define recvfrom(s, buf, len, flags, src, slen)     x_recvfrom(s, buf, len, flags, src, slen)
#define setsockopt(s, level, optname, optval, len)  x_setsockopt(s, level, optname, optval, len)
#define close(s)                                    x_close(s)
#define connect(s, addr, len)                       x_connect(s, addr, len)
#define send(s, buf, len, flags)                    x_send(s, buf, len, flags)
#define recv(s, buf, len, flags)                    x_recv(s, buf, len, flags)
#define bind(s, addr, len)                          x_bind(s, addr, len)
#define listen(s, backlog)                          x_listen(s, backlog)
#define accept(s, addr, len)                        x_accept(s, addr, len)

#endif //EASY_NET_NET_API_H
