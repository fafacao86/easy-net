#include "utils.h"
#include "log.h"
#include "ipaddr.h"

static int is_little_endian(void) {
    // big endian：0x12, 0x34; small endian：0x34, 0x12
    uint16_t v = 0x1234;
    uint8_t* b = (uint8_t*)&v;
    return b[0] == 0x34;
}


net_err_t utils_init(void) {
    log_info(LOG_UTILS, "init tools.");

    // the endian of the host and the settings must be the same.
    int host_endian = is_little_endian();
    log_info(LOG_UTILS, "host endian: %d", host_endian);
    if (host_endian  != NET_ENDIAN_LITTLE) {
        log_error(LOG_UTILS, "check endian faild.");
        return NET_ERR_SYS;
    }
    log_info(LOG_UTILS, "done.");
    return NET_OK;
}


/**
 * https://www.youtube.com/watch?v=_zMf4KYoKbM
 * */
uint16_t checksum16(uint32_t offset, void* buf, uint16_t len, uint32_t pre_sum, int complement) {
    uint16_t* curr_buf = (uint16_t *)buf;
    uint32_t checksum = pre_sum;
    if (offset & 0x1) {
        uint8_t * buf = (uint8_t *)curr_buf;
        checksum += *buf++ << 8;
        curr_buf = (uint16_t *)buf;
        len--;
    }

    while (len > 1) {
        checksum += *curr_buf++;
        len -= 2;
    }

    if (len > 0) {
        checksum += *(uint8_t*)curr_buf;
    }
    uint16_t high;
    while ((high = checksum >> 16) != 0) {
        checksum = high + (checksum & 0xffff);
    }

    return complement ? (uint16_t)~checksum : (uint16_t)checksum;
}



uint16_t checksum_peso(const uint8_t * src_ip, const uint8_t* dest_ip, uint8_t protocol, packet_t * buf) {
    uint8_t zero_protocol[2] = { 0, protocol };
    uint16_t len = e_htons(buf->total_size);
    int offset = 0;
    uint32_t sum = checksum16(offset, (uint16_t*)src_ip, IPV4_ADDR_SIZE, 0, 0);
    offset += IPV4_ADDR_SIZE;
    sum = checksum16(offset, (uint16_t*)dest_ip, IPV4_ADDR_SIZE, sum, 0);
    offset += IPV4_ADDR_SIZE;
    sum = checksum16(offset, (uint16_t*)zero_protocol, 2, sum, 0);
    offset += 2;
    sum = checksum16(offset, (uint16_t*)&len, 2, sum, 0);
    packet_reset_pos(buf);
    sum = packet_checksum16(buf, buf->total_size, sum, 1);
    return sum;
}