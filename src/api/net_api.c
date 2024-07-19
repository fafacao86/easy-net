#include "utils.h"
#include "sys_plat.h"
#include "socket.h"
#define IPV4_STR_SIZE           16

/**
 * x_in_addr to string
 * not reentrant safe
 */
char* x_inet_ntoa(struct x_in_addr in) {
    static char buf[IPV4_STR_SIZE];
    plat_sprintf(buf, "%d.%d.%d.%d", in.addr0, in.addr1, in.addr2, in.addr3);
    return buf;
}

/**
 * string to 32-bit integer
 */
uint32_t x_inet_addr(const char* str) {
    if (!str) {
        return INADDR_ANY;
    }

    ipaddr_t ipaddr;
    ipaddr_from_str(&ipaddr, str);
    return ipaddr.q_addr;
}

/**
 * string to x_in_addr
 * 1 for success, 0 for invalid input, -1 for failure
 * reentrant safe
 */
int x_inet_pton(int family, const char *strptr, void *addrptr) {
    // only support IPv4
    if ((family != AF_INET) || !strptr || !addrptr) {
        return -1;
    }
    struct x_in_addr * addr = (struct x_in_addr *)addrptr;

    ipaddr_t dest;
    ipaddr_from_str(&dest, strptr);
    addr->s_addr = dest.q_addr;
    return 1;
}

/**
 * x_in_addr to string
 * reentrant safe
 */
const char * x_inet_ntop(int family, const void *addrptr, char *strptr, size_t len) {
    if ((family != AF_INET) || !addrptr || !strptr || !len) {
        return (const char *)0;
    }

    struct x_in_addr * addr = (struct x_in_addr *)addrptr;
    char buf[IPV4_STR_SIZE];
    plat_sprintf(buf, "%d.%d.%d.%d", addr->addr0, addr->addr1, addr->addr2, addr->addr3);
    plat_strncpy(strptr, buf, len - 1);
    strptr[len - 1] = '\0';
    return strptr;
}
