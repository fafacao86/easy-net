#ifndef EASY_NET_PACKET_BUFFER_H
#define EASY_NET_PACKET_BUFFER_H

#include <stdint.h>
#include "list.h"
#include "easy_net_config.h"
#include "net_errors.h"

/**
 * Page of a packet
 * Each page has a fixed size payload buffer,
 * the data is a continuous block of memory starting from the data pointer.
 */
typedef struct page_t {
    list_node_t node;
    int size;                               // size of the data in this page
    uint8_t* data;                          // starting address of the data in this page
    uint8_t payload[PACKET_PAGE_SIZE];       // data buffer
} page_t;


/**
 * A packet consists of a list of pages
 * The packet provides pos pointer to abstract away the details of the page list
 */
typedef struct packet_t {
    int total_size;
    list_t page_list;
    list_node_t node;

    int ref;                                // reference counter
    int pos;                                // current offset in the packet
    page_t* cur_page;                     // the page that pos pointer is currently in
    uint8_t* page_offset;                    // the offset in the current page
} packet_t;


static inline page_t * page_next(page_t * page) {
    list_node_t * next = list_node_next(&page->node);
    return list_entry(next, page_t, node);
}


static inline page_t * packet_first_page(packet_t * packet) {
    list_node_t * first = list_first(&packet->page_list);
    return list_entry(first, page_t, node);
}

static inline page_t * packet_last_page(packet_t * buf) {
    list_node_t * first = list_last(&buf->page_list);
    return list_entry(first, page_t, node);
}

static int inline packet_total_size (packet_t * packet) {
    return packet->total_size;
}

static inline uint8_t * packet_data (packet_t * packet) {
    page_t * first = packet_first_page(packet);
    return first ? first->data : (uint8_t *)0;
}

void packet_buffer_mem_stat(void);
net_err_t packet_buffer_init(void);
packet_t * packet_alloc(int size);


#endif //EASY_NET_PACKET_BUFFER_H
