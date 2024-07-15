#include "testcase.h"
void test_arp(netif_t * netif){
    packet_t * packet = packet_alloc(32);
    packet_fill(packet, 0x53, 32);
    ipaddr_t dest;
    ipaddr_from_str(&dest, friend0_ip);
    netif_out(netif, &dest, packet);
}
