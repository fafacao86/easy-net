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

void packet_inc_ref (packet_t * packet){
    locker_lock(&locker);
    packet->ref++;
    locker_unlock(&locker);
}

static inline int curr_page_tail_free(page_t* page) {
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
            log_error(LOG_PACKET_BUFFER, "bad page data. ");
        }

        int pre_size = (int)(curr->data - curr->payload);
        plat_printf("Pre:%d b, ", pre_size);

        int used_size = curr->size;
        plat_printf("Used:%d b, ", used_size);

        int free_size = curr_page_tail_free(curr);
        plat_printf("Free:%d b, ", free_size);
        plat_printf("\n");

        int blk_total = pre_size + used_size + free_size;
        if (blk_total != PACKET_PAGE_SIZE) {
            log_error(LOG_PACKET_BUFFER,"bad page size. %d != %d", blk_total, PACKET_PAGE_SIZE);
        }
        total_size += used_size;
    }
    if (total_size != buf->total_size) {
        log_error(LOG_PACKET_BUFFER,"bad packet size. %d != %d", total_size, buf->total_size);
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

static void page_free (page_t * page) {
    memory_pool_free(&page_pool, page);
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

/**
 * Add header to the packet
 * If cont is 1, then the header will be guaranteed not to be split into multiple pages.
 * If cont is 0, then the header may be split into multiple pages.
 * */
net_err_t packet_add_header(packet_t * packet, int size, int cont){
    assert_halt(packet->ref != 0, "packet freed");
    page_t * page = packet_first_page(packet);

    // if the first page has enough space, just add the header
    int resv_size = (int)(page->data - page->payload);
    if (size <= resv_size) {
        page->size += size;
        page->data -= size;
        packet->total_size += size;

        display_check_buf(packet);
        return NET_OK;
    }

    if (cont) {
        // if cont is 1, allocate a new page for the header
        if (size > PACKET_PAGE_SIZE) {
            log_error(LOG_PACKET_BUFFER,"is_contious && size too big %d > %d", size, PACKET_PAGE_SIZE);
            return NET_ERR_SIZE;
        }

        page = page_alloc_list(size, 1);
        if (!page) {
            log_error(LOG_PACKET_BUFFER,"no buffer for alloc(size:%d)", size);
            return NET_ERR_MEM;
        }
    } else {
        // if cont is 0, utilize the remaining space in the first page
        page->data = page->payload;
        page->size += resv_size;
        packet->total_size += resv_size;
        size -= resv_size;

        // then allocate a new page for the rest of the header
        page = page_alloc_list(size, 1);
        if (!page) {
            log_error(LOG_PACKET_BUFFER,"no buffer for alloc(size:%d)", size);
            return NET_ERR_MEM;
        }
    }

    packet_insert_page_list(packet, page, 0);
    display_check_buf(packet);
    return NET_OK;
}

/**
 * Remove header from the packet
 * Since the header size is known, this function is easier to implement than add_header.
 * */
net_err_t packet_remove_header(packet_t* packet, int size){
    assert_halt(packet->ref != 0, "packet freed");
    page_t* page = packet_first_page(packet);
    while (size) {
        page_t * next_pg = page_next(page);

        if (size < page->size) {
            page->data += size;
            page->size -= size;
            packet->total_size -= size;
            break;
        }

        int curr_size = page->size;

        list_remove_first(&packet->page_list);
        page_free(page);

        size -= curr_size;
        packet->total_size -= curr_size;
        page = next_pg;
    }
    display_check_buf(packet);
    return NET_OK;
}


/**
 * Resize the packet to a new size. Either increase or decrease the size.
 * */
net_err_t packet_resize(packet_t * packet, int to_size){
    assert_halt(packet->ref != 0, "packet freed");
    // if the packet is already the desired size, do nothing
    if (to_size == packet->total_size) {
        return NET_OK;
    }

    if (packet->total_size == 0) {
        // if the packet is empty, just call page_alloc_list
        page_t* page = page_alloc_list(to_size, 0);
        if (!page) {
            log_error(LOG_PACKET_BUFFER, "not enough pages.");
            return NET_ERR_MEM;
        }
        packet_insert_page_list(packet, page, 1);
    } else if (to_size == 0) {
        page_free_list(packet_first_page(packet));
        packet->total_size = 0;
        init_list(&packet->page_list);
    }  else if (to_size > packet->total_size) {
        // tail insertion
        page_t * tail_page = packet_last_page(packet);

        int inc_size = to_size - packet->total_size;
        int remain_size = curr_page_tail_free(tail_page);
        if (remain_size >= inc_size) {
            // if the tail page has enough space, just increase the size
            tail_page->size += inc_size;
            packet->total_size += inc_size;
        } else {
            //
            page_t * new_pg = page_alloc_list(inc_size - remain_size, 0);
            if (!new_pg) {
                log_error(LOG_PACKET_BUFFER, "not enough pages.");
                return NET_ERR_MEM;
            }
            tail_page->size += remain_size;
            packet->total_size += remain_size;
            packet_insert_page_list(packet, new_pg, 1);
        }
    } else {
        int total_size = 0;
        // iterate through the pages till the desired size is reached
        page_t* tail_page;
        for (tail_page = packet_first_page(packet); tail_page; tail_page = page_next(tail_page)) {
            total_size += tail_page->size;
            if (total_size >= to_size) {
                break;
            }
        }

        if (tail_page == (page_t*)0) {
            return NET_ERR_SIZE;
        }
        // delete the pages after the tail page
        page_t * curr_pg = page_next(tail_page);
        total_size = 0;
        while (curr_pg) {
            page_t * next_blk = page_next(curr_pg);
            list_remove(&packet->page_list, &curr_pg->node);
            page_free(curr_pg);
            total_size += curr_pg->size;
            curr_pg = next_blk;
        }

        tail_page->size -= packet->total_size - total_size - to_size;
        packet->total_size = to_size;
    }
    display_check_buf(packet);
    return NET_OK;
}

void packet_free (packet_t * pkt){
    locker_lock(&locker);
    if (--pkt->ref == 0) {
        page_free_list(packet_first_page(pkt));
        memory_pool_free(&packet_pool, pkt);
    }
    locker_unlock(&locker);
}

net_err_t packet_join(packet_t* dest, packet_t* src){
    assert_halt(dest->ref != 0, "packet freed");
    assert_halt(src->ref != 0, "packet freed");
    page_t* first;
    while ((first = packet_first_page(src))) {
        list_remove_first(&src->page_list);
        packet_insert_page_list(dest, first, 1);
    }
    packet_free(src);
    log_info(LOG_PACKET_BUFFER,"join result:");
    display_check_buf(dest);
    return NET_OK;
}

/**
 * Make the first size bytes of the packet continuous.
 * **/
net_err_t packet_set_cont(packet_t* buf, int size){
    assert_halt(buf->ref != 0, "packet freed")
    // 必须要有足够的长度
    if (size > buf->total_size) {
        log_error(LOG_PACKET_BUFFER,"size(%d) > total_size(%d)", size, buf->total_size);
        return NET_ERR_SIZE;
    }

    if (size > PACKET_PAGE_SIZE) {
        log_error(LOG_PACKET_BUFFER,"size too big > %d", LOG_PACKET_BUFFER);
        return NET_ERR_SIZE;
    }

    // if it is already continuous, do nothing
    page_t * first_pg = packet_first_page(buf);
    if (size <= first_pg->size) {
        display_check_buf(buf);
        return NET_OK;
    }

#if 0
    uint8_t * dest = first_pg->payload + PKTBUF_BLK_SIZE - size;
    plat_memmove(dest, first_pg->data, first_pg->size);
    first_pg->data = dest;
    dest += first_pg->size;
#else
    uint8_t * dest = first_pg->payload;
    for (int i = 0; i < first_pg->size; i++) {
        *dest++ = first_pg->data[i];
    }
    first_pg->data = first_pg->payload;
#endif

    int remain_size = size - first_pg->size;
    page_t * curr_blk = page_next(first_pg);
    while (remain_size && curr_blk) {
        int curr_size = (curr_blk->size > remain_size) ? remain_size : curr_blk->size;
        plat_memcpy(dest, curr_blk->data, curr_size);
        dest += curr_size;
        curr_blk->data += curr_size;
        curr_blk->size -= curr_size;
        first_pg->size += curr_size;
        remain_size -= curr_size;
        if (curr_blk->size == 0) {
            page_t * next_pg = page_next(curr_blk);

            list_remove(&buf->page_list, &curr_blk->node);
            page_free(curr_blk);
            curr_blk = next_pg;
        }
    }
    display_check_buf(buf);
    return NET_OK;
}


static int curr_page_remain(packet_t * packet) {
    page_t* page = packet->cur_page;
    if (!page) {
        return 0;
    }

    return (int)(packet->cur_page->data + page->size - packet->page_offset);
}

/**
 * move the pos and page_offset
 * if cross pages, only move to the starting point of next page
 * The starting point property must be ensured by the caller.
 * */
static void move_forward(packet_t* buf, int size) {
    page_t* curr = buf->cur_page;

    buf->pos += size;
    buf->page_offset += size;

    // offset might be beyond the end of the current block, move to the next block
    if (buf->page_offset >= curr->data + curr->size) {
        buf->cur_page = page_next(curr);
        if (buf->cur_page) {
            buf->page_offset = buf->cur_page->data;
        } else {
            buf->page_offset = (uint8_t*)0;
        }
    }
}

net_err_t packet_seek(packet_t* packet, int offset){
    assert_halt(packet->ref != 0, "packet freed")
    if (packet->pos == offset) {
        return NET_OK;
    }
    if ((offset < 0) || (offset >= packet->total_size)) {
        return NET_ERR_SIZE;
    }

    int move_bytes;
    if (offset < packet->pos) {
        // move backward is a bit hard, here we just reset the position, and move forward
        packet->cur_page = packet_first_page(packet);
        packet->page_offset = packet->cur_page->data;
        packet->pos = 0;

        move_bytes = offset;
    } else {
        move_bytes = offset - packet->pos;
    }

    while (move_bytes) {
        int remain_size = curr_page_remain(packet);
        int curr_move = move_bytes > remain_size ? remain_size : move_bytes;
        move_forward(packet, curr_move);
        move_bytes -= curr_move;
    }
    return NET_OK;
}

/**
 * Bytes remain starting from the current position.
 * */
static inline int total_blk_remain(packet_t* packet) {
    return packet->total_size - packet->pos;
}

/**
 * write beginning from the current position
 * */
int packet_write(packet_t * packet, uint8_t* src, int size){
    assert_halt(packet->ref != 0, "packet freed")
    if (!src || !size) {
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain(packet);
    if (remain_size < size) {
        log_error(LOG_PACKET_BUFFER, "size errorL %d < %d", remain_size, size);
        return NET_ERR_SIZE;
    }

    while (size > 0) {
        int page_size = curr_page_remain(packet);
        int curr_copy = size > page_size ? page_size : size;
        plat_memcpy(packet->page_offset, src, curr_copy);
        move_forward(packet, curr_copy);
        src += curr_copy;
        size -= curr_copy;
    }

    return NET_OK;
}

/**
 * read from the current position
 * */
int packet_read(packet_t* packet, uint8_t* dest, int size){
    assert_halt(packet->ref != 0, "packet freed")

    if (!dest || !size) {
        return NET_OK;
    }

    int remain_size = total_blk_remain(packet);
    if (remain_size < size) {
        log_error(LOG_PACKET_BUFFER, "size errorL %d < %d", remain_size, size);
        return NET_ERR_SIZE;
    }

    while (size > 0) {
        int blk_size = curr_page_remain(packet);
        int curr_copy = size > blk_size ? blk_size : size;
        plat_memcpy(dest, packet->page_offset, curr_copy);
        move_forward(packet, curr_copy);\
        dest += curr_copy;
        size -= curr_copy;
    }

    return NET_OK;
}


net_err_t packet_copy(packet_t * dest, packet_t* src, int size){
    assert_halt(src->ref != 0, "buf freed")
    assert_halt(dest->ref != 0, "buf freed")

    if ((total_blk_remain(dest) < size) || (total_blk_remain(src) < size)) {
        return NET_ERR_SIZE;
    }
    while (size) {
        int dest_remain = curr_page_remain(dest);
        int src_remain = curr_page_remain(src);
        int copy_size = dest_remain > src_remain ? src_remain : dest_remain;
        copy_size = copy_size > size ? size : copy_size;

        plat_memcpy(dest->page_offset, src->page_offset, copy_size);

        move_forward(dest, copy_size);
        move_forward(src, copy_size);
        size -= copy_size;
    }
    return NET_OK;
}

/**
 * fill size bytes starting from the current position with val.
 * */
net_err_t packet_fill(packet_t* packet, uint8_t val, int size){
    assert_halt(packet->ref != 0, "buf freed")

    if (!size) {
        return NET_ERR_PARAM;
    }
    int remain_size = total_blk_remain(packet);
    if (remain_size < size) {
        log_error(LOG_PACKET_BUFFER, "size errorL %d < %d", remain_size, size);
        return NET_ERR_SIZE;
    }
    while (size > 0) {
        int blk_size = curr_page_remain(packet);
        int curr_fill = size > blk_size ? blk_size : size;
        plat_memset(packet->page_offset, val, curr_fill);
        move_forward(packet, curr_fill);
        size -= curr_fill;
    }

    return NET_OK;
}

