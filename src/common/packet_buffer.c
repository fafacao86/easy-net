#include "packet_buffer.h"
#include "memory_pool.h"
#include "list.h"
#include "locker.h"
#include "log.h"
#include "net_errors.h"
#include "easy_net_config.h"

static locker_t locker;                    // for allocation and de-allocation
static memory_pool_t page_pool;            // for page allocation
static page_t page_buffer[PACKET_PAGE_CNT];
static memory_pool_t packet_pool;          // for packet allocation
static packet_t packet_buffer[PACKET_BUFFER_SIZE];

static page_t * page_alloc(void) {
    locker_lock(&locker);
    page_t * page = memory_pool_alloc(&page_pool, -1);
    locker_unlock(&locker);

    if (page) {
        page->size = 0;
        page->data = (uint8_t *)0;
        list_node_init(&page->node);
    }

    return page;
}

static void page_free_list(page_t* first) {
    while (first) {
        page_t* next_block = page_next(first);
        memory_pool_free(&page_pool, first);
        first = next_block;
    }
}

/**
 * Allocate a list of pages from the page buffer pool
 * when the add_front is 1, the list is allocated using head insertion
 * when the add_front is 0, the list is allocated using tail insertion
 * */
static page_t* page_alloc_list(int size, int add_front) {
    page_t* first_page = (page_t*)0;
    page_t* pre_page = (page_t*)0;

    while (size) {
        page_t* new_page = page_alloc();
        if (!new_page) {
            log_error(LOG_PACKET_BUFFER, "no buffer for alloc(size:%d)", size);
            if (first_page) {
                // if failed, free all already allocated pages
                locker_lock(&locker);
                page_free_list(first_page);
                locker_unlock(&locker);
            }
            return (page_t*)0;
        }
        int curr_size = 0;
        if (add_front) {
            curr_size = size > PACKET_PAGE_SIZE ? PACKET_PAGE_SIZE : size;

            // head insertion, so the data is close to the end of the payload
            new_page->size = curr_size;
            new_page->data = new_page->payload + PACKET_PAGE_SIZE - curr_size;
            if (first_page) {
                list_node_set_next(&new_page->node, &first_page->node);
            }

            first_page = new_page;
        } else {
            if (!first_page) {
                first_page = new_page;
            }

            curr_size = size > PACKET_PAGE_SIZE ? PACKET_PAGE_SIZE : size;

            new_page->size = curr_size;
            new_page->data = new_page->payload;
            if (pre_page) {
                list_node_set_next(&pre_page->node, &new_page->node);
            }
        }
        size -= curr_size;
        pre_page = new_page;
    }
    return first_page;
}

static void packet_insert_page_list(packet_t * packet, page_t * first_page, int add_last) {
    if (add_last) {
        // append the page list to the end of the packet pages
        while (first_page) {
            page_t * next_blk = page_next(first_page);
            list_insert_last(&packet->page_list, &first_page->node);
            packet->total_size += first_page->size;
            first_page = next_blk;
        }
    } else {
        // prepend the page list to the beginning of the packet pages
        page_t * pre = (page_t*)0;
        while (first_page) {
            page_t *next = page_next(first_page);
            if (pre) {
                list_insert_after(&packet->page_list, &pre->node, &first_page->node);
            } else {
                list_insert_first(&packet->page_list, &first_page->node);
            }
            packet->total_size += first_page->size;
            pre = first_page;
            first_page = next;
        };
    }
}

void packet_reset_pos(packet_t* packet) {
    if (packet) {
        packet->pos = 0;
        packet->cur_page = packet_first_page(packet);
        packet->page_offset = packet->cur_page ? packet->cur_page->data : (uint8_t*)0;
    }
}

static inline int curr_blk_tail_free(page_t* page) {
    return PACKET_PAGE_SIZE - (int)(page->data - page->payload) - page->size;
}

#if LOG_DISP_ENABLED(LOG_PACKET_BUFFER)
static void display_check_buf(packet_t * buf) {
    if (!buf) {
        log_error(LOG_PACKET_BUFFER, "invalid buf. buf == 0");
        return;
    }

    plat_printf("check buf %p: size %d\n", buf, buf->total_size);
    page_t* curr;
    int total_size = 0, index = 0;
    for (curr = packet_first_page(buf); curr; curr = page_next(curr)) {
        plat_printf("%d: ", index++);

        if ((curr->data < curr->payload) || (curr->data >= curr->payload + PACKET_PAGE_SIZE)) {
            log_error(LOG_PACKET_BUFFER, "bad block data. ");
        }

        int pre_size = (int)(curr->data - curr->payload);
        plat_printf("Pre:%d b, ", pre_size);

        // 中间存在的已用区域
        int used_size = curr->size;
        plat_printf("Used:%d b, ", used_size);

        // 末尾可能存在的未用区域
        int free_size = curr_blk_tail_free(curr);
        plat_printf("Free:%d b, ", free_size);
        plat_printf("\n");

        // 检查当前包的大小是否与计算的一致
        int blk_total = pre_size + used_size + free_size;
        if (blk_total != PACKET_PAGE_SIZE) {
            log_error(LOG_PACKET_BUFFER,"bad block size. %d != %d", blk_total, PACKET_PAGE_SIZE);
        }
        total_size += used_size;
    }

    if (total_size != buf->total_size) {
        log_error(LOG_PACKET_BUFFER,"bad buf size. %d != %d", total_size, buf->total_size);
    }
}
#else
#define display_check_buf(buf)
#endif

void packet_buffer_mem_stat(void){
    log_info(LOG_PACKET_BUFFER,"packet buffer mem stat: page_pool:%d/%d, packet_pool:%d/%d",
        memory_pool_free_cnt(&page_pool), PACKET_PAGE_CNT,
        memory_pool_free_cnt(&page_pool), PACKET_BUFFER_SIZE);
}


net_err_t packet_buffer_init(void) {
    log_info(LOG_PACKET_BUFFER,"init packet buffer.");
    locker_init(&locker, LOCKER_THREAD);
    memory_pool_init(&page_pool, page_buffer, sizeof(page_t), PACKET_PAGE_CNT, LOCKER_NONE);
    memory_pool_init(&packet_pool, packet_buffer, sizeof(packet_t), PACKET_BUFFER_SIZE, LOCKER_NONE);
    log_info(LOG_PACKET_BUFFER,"init done.");
    return NET_OK;
}


/**
 * Allocate a packet from the packet buffer pool
 * */
packet_t * packet_alloc(int size){
    locker_lock(&locker);
    packet_t * pkt = memory_pool_alloc(&packet_pool, -1);
    locker_unlock(&locker);
    if (!pkt) {
        log_error(LOG_PACKET_BUFFER, "no packet available.");
        return (packet_t*)0;
    }

    pkt->ref = 1;
    pkt->total_size = 0;
    init_list(&pkt->page_list);
    list_node_init(&pkt->node);

    // allocate the pages
    if (size) {
        page_t* page = page_alloc_list(size, 0);
        if (!page) {
            memory_pool_free(&packet_pool, pkt);
            return (packet_t*)0;
        }
        packet_insert_page_list(pkt, page, 1);
    }

    packet_reset_pos(pkt);

    display_check_buf(pkt);
    return pkt;
}

