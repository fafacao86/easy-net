#include "ipaddr.h"

void ipaddr_set_any(ipaddr_t * ip) {
    ip->q_addr = 0;
}

/**
 * get ip address in string format
 */
net_err_t ipaddr_from_str(ipaddr_t * dest, const char* str) {
    if (!dest || !str) {
        return NET_ERR_PARAM;
    }
    dest->q_addr = 0;
    char c;
    uint8_t * p = dest->a_addr;
    uint8_t sub_addr = 0;
    while ((c = *str++) != '\0') {
        if ((c >= '0') && (c <= '9')) {
            sub_addr = sub_addr * 10 + c - '0';
        } else if (c == '.') {
            *p++ = sub_addr;
            sub_addr = 0;
        } else {
            return NET_ERR_PARAM;
        }
    }
    *p++ = sub_addr;
    return NET_OK;
}

/**
 * generate a blank ip address
 */
ipaddr_t * ipaddr_get_any(void) {
    static ipaddr_t ipaddr_any = { .q_addr = 0 };
    return &ipaddr_any;
}

/**
 * copy ip address from src to dest
 */
void ipaddr_copy(ipaddr_t * dest, const ipaddr_t * src) {
    if (!dest || !src) {
        return;
    }
    dest->q_addr = src->q_addr;
}


int ipaddr_is_equal(const ipaddr_t * ipaddr1, const ipaddr_t * ipaddr2) {
    return ipaddr1->q_addr == ipaddr2->q_addr;
}

void ipaddr_to_buf(const ipaddr_t* src, uint8_t* ip_buf) {
    ip_buf[0] = src->a_addr[0];
    ip_buf[1] = src->a_addr[1];
    ip_buf[2] = src->a_addr[2];
    ip_buf[3] = src->a_addr[3];
}
