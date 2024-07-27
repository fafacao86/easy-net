#ifndef EASY_NET_TCP_BUF_H
#define EASY_NET_TCP_BUF_H


#include <stdint.h>
#include "packet_buffer.h"

/**
 * circular buffer for tcp data
 */
typedef struct _tcp_sbuf_t {
    int count;
    int in, out;                        // out ....... in
    int size;
    uint8_t * data;
}tcp_buf_t;

// initialize the buffer when connect
void tcp_buf_init(tcp_buf_t* buf, uint8_t * data, int size);

void tcp_buf_write_send(tcp_buf_t * dest, const uint8_t * buffer, int len);
void tcp_buf_read_send(tcp_buf_t * src, int offset, packet_t * dest, int count);

static inline int tcp_buf_size (tcp_buf_t * buf) {
    return buf->size;
}

static inline int tcp_buf_free_cnt(tcp_buf_t * buf) {
    return buf->size - buf->count;
}

static inline int tcp_buf_cnt (tcp_buf_t * buf) {
    return buf->count;
}
int tcp_buf_remove(tcp_buf_t * buf, int cnt);

#endif //EASY_NET_TCP_BUF_H
