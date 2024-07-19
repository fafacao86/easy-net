#ifndef EASY_NET_UTILS_H
#define EASY_NET_UTILS_H

#include <stdint.h>
#include <easy_net_config.h>
#include "net_errors.h"

/**
 * swap the bit pattern of a 16-bit value
 */
static inline uint16_t swap_u16(uint16_t v) {
    // 7..0 => 15..8, 15..8 => 7..0
    uint16_t r = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
    return r;
}

/**
 * swap the bit pattern of a 32-bit value
 */
static inline uint32_t swap_u32(uint32_t v) {
    uint32_t r =
            (((v >>  0) & 0xFF) << 24)    // 0..7 -> 24..31
            | (((v >>  8) & 0xFF) << 16)    // 8..15 -> 16..23
            | (((v >> 16) & 0xFF) << 8)     // 16..23 -> 8..15
            | (((v >> 24) & 0xFF) << 0);    // 24..31 -> 0..7
    return r;
}

#if NET_ENDIAN_LITTLE
uint16_t swap_u16(uint16_t v);
uint32_t swap_u32(uint32_t v);
#define e_htons(v)        swap_u16(v)
#define e_ntohs(v)        swap_u16(v)
#define e_htonl(v)        swap_u32(v)
#define e_ntohl(v)        swap_u32(v)

#else
// if the system is big-endian, no need to convert
#define e_htons(v)        (v)
#define e_ntohs(v)        (v)
#define e_htonl(v)        (v)
#define e_ntohl(v)        (v)
#endif

net_err_t utils_init(void);
uint16_t checksum16(uint32_t offset, void* buf, uint16_t len, uint32_t pre_sum, int complement);

#endif //EASY_NET_UTILS_H
