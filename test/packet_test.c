#include "packet_buffer.h"
#include "sys_plat.h"

void test_packet_buffer(){
    packet_buffer_init();
    static uint16_t temp[1000];
    static uint16_t read_temp[1000];

    for (int i = 0; i < 1024; i++) {
        temp[i] = i;
    }
    packet_t* pkt = packet_alloc(64);
    packet_buffer_mem_stat();
}