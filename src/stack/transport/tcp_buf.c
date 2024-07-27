#include "tcp_buf.h"
#include "log.h"

void tcp_buf_init(tcp_buf_t* buf, uint8_t * data, int size) {
    buf->in = buf->out = 0;
    buf->count = 0;
    buf->size = size;
    buf->data = data;
}


void tcp_buf_write_send(tcp_buf_t * dest, const uint8_t * buffer, int len) {
    while (len > 0) {
        dest->data[dest->in++] = *buffer++;
        if (dest->in >= dest->size) {
            dest->in = 0;
        }

        dest->count++;
        len--;
    }
}


/**
 * start from offset, read count bytes from buffer and write to dest packet
 */
void tcp_buf_read_send(tcp_buf_t * buf, int offset, packet_t * dest, int count) {
    int free_for_us = buf->count - offset;
    if (count > free_for_us) {
        log_warning(LOG_TCP, "resize for send: %d -> %d", count, free_for_us);
        count = free_for_us;
    }

    // be careful with wrap around
    int start = buf->out + offset;
    if (start >= buf->size) {
        start -= buf->size;
    }

    while (count > 0) {
        int end = start + count;
        if (end >= buf->size) {
            end = buf->size;
        }
        int copy_size = (int)(end - start);
        net_err_t err = packet_write(dest, buf->data + start, (int)copy_size);
        assert_halt(err >= 0, "write buffer failed.");
        start += copy_size;
        if (start >= buf->size) {
            start -= buf->size;
        }
        count -= copy_size;
    }
}