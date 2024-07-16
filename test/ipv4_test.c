#include "testcase.h"

void test_ipv4(){
    packet_t * buf = packet_alloc(32);
    packet_fill(buf, 0xA5, buf->total_size);

    ipaddr_t dest, src;
    ipaddr_from_str(&dest, friend0_ip);
    ipaddr_from_str(&src, netdev0_ip);
    ipv4_out(0, &dest, &src, buf);
}