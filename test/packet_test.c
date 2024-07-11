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
    packet_add_header(pkt, 10, 1);
    packet_add_header(pkt, 20, 1);
    packet_buffer_mem_stat();
    packet_remove_header(pkt, 40);
    packet_buffer_mem_stat();
    packet_resize(pkt, 300);

    packet_join(pkt, packet_alloc(44));

    packet_resize(pkt, 600);
    packet_buffer_mem_stat();
    packet_add_header(pkt, 12, CONTINUOUS);
    packet_set_cont(pkt, 24);

    packet_reset_pos(pkt);
    packet_write(pkt, (uint8_t *)temp, packet_total_size(pkt));
    plat_memset(read_temp, 0, sizeof(read_temp));
    packet_reset_pos(pkt);
    packet_read(pkt, (uint8_t *)read_temp, packet_total_size(pkt));
    for(int i = 0; i < 1000; i++){
        printf("%d ", read_temp[i]);
    }
}