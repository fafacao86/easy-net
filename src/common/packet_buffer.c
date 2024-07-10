#include "locker.h"
#include "packet_buffer.h"
#include "log.h"
#include "memory_pool.h"

#define DBG_DISP_ENABLED(DBG_BUF)

static locker_t locker;                    // 分配与回收的锁
static memory_pool_t block_list;                 // 空闲包列表
static page_t block_buffer[PACKET_PAGE_SIZE];
static memory_pool_t pktbuf_list;                // 空闲包列表
static packet_t pktbuf_buffer[PACKET_PAGE_SIZE];

/**
 * 获取blk的剩余空间大小
 */
static inline int curr_blk_tail_free(page_t* blk) {
    return PACKET_PAGE_SIZE - (int)(blk->data - blk->payload) - blk->size;
}

/**
 * 打印缓冲表链、同时检查表链的是否正确配置
 *
 * 在打印的过程中，同时对整个缓存链表进行检查，看看是否存在错误
 * 主要通过检查空间和size的设置是否正确来判断缓存是否正确设置
 *
 * @param buf 待查询的Buf
 */
#if DBG_DISP_ENABLED(DBG_BUF)
static void display_check_buf(packet_t* buf) {
    if (!buf) {
        dbg_error(DBG_BUF, "invalid buf. buf == 0");
        return;
    }

    plat_printf("check buf %p: size %d\n", buf, buf->total_size);
    page_t* curr;
    int total_size = 0, index = 0;
    for (curr = pktbuf_first_blk(buf); curr; curr = pktbuf_blk_next(curr)) {
        plat_printf("%d: ", index++);

        if ((curr->data < curr->payload) || (curr->data >= curr->payload + PKTBUF_BLK_SIZE)) {
            dbg_error(DBG_BUF, "bad block data. ");
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
        if (blk_total != PKTBUF_BLK_SIZE) {
            dbg_error(DBG_BUF,"bad block size. %d != %d", blk_total, PKTBUF_BLK_SIZE);
        }

        // 累计总的大小
        total_size += used_size;
    }

    // 检查总的大小是否一致
    if (total_size != buf->total_size) {
        dbg_error(DBG_BUF,"bad buf size. %d != %d", total_size, buf->total_size);
    }
}
#else
#define display_check_buf(buf)
#endif

/**
 * @brief 分配一个空闲的block
 */
static page_t* pktblock_alloc(void) {
    // 不等待分配，因为会在中断中调用
    locker_lock(&locker);
    page_t* block = mblock_alloc(&block_list, -1);
    locker_unlock(&locker);

    if (block) {
        block->size = 0;
        block->data = (uint8_t *)0;
        list_node_init(&block->node);
    }

    return block;
}

/**
 * @brief 分配一个缓冲链
 *
 * 由于分配是以BUF块为单位，所以alloc_size的大小可能会小于实际分配的BUF块的总大小
 * 那么此时就有一部分空间未用，这部分空间可能放在链表的最开始，也可能放在链表的结尾处
 * 具体存储，取决于add_front，add_front=1，将新分配的块插入到表头.否则，插入到表尾
 *
 */
static page_t* pktblock_alloc_list(int size, int add_front) {
    page_t* first_block = (page_t*)0;
    page_t* pre_block = (page_t*)0;

    while (size) {
        // 分配一个block，大小为0
        page_t* new_block = pktblock_alloc();
        if (!new_block) {
            log_error(LOG_PACKET_BUFFER, "no buffer for alloc(size:%d)", size);
            if (first_block) {
                // 失败，要回收释放整个链
                //pktblock_free_list(first_block);
            }
            return (page_t*)0;
        }

        int curr_size = 0;
        if (add_front) {
            curr_size = size > LOG_PACKET_BUFFER ? LOG_PACKET_BUFFER : size;

            // 反向分配，从末端往前分配空间
            new_block->size = curr_size;
            new_block->data = new_block->payload + LOG_PACKET_BUFFER - curr_size;
            if (first_block) {
                // 将自己加在头部
                list_node_set_next(&new_block->node, &first_block->node);
            }

            // 如果是反向分配，第一个包总是当前分配的包
            first_block = new_block;
        } else {
            // 如果是正向分配，第一个包是第1个分配的包
            if (!first_block) {
                first_block = new_block;
            }

            curr_size = size > LOG_PACKET_BUFFER ? LOG_PACKET_BUFFER : size;

            // 正向分配，从前端向末端分配空间
            new_block->size = curr_size;
            new_block->data = new_block->payload;
            if (pre_block) {
                // 将自己添加到表尾
                list_node_set_next(&pre_block->node, &new_block->node);
            }
        }

        size -= curr_size;
        pre_block = new_block;
    }

    return first_block;
}

/**
 * 将Block链表插入到buf中
 */
static void pktbuf_insert_blk_list(packet_t * buf, page_t * first_blk, int add_last) {
    if (add_last) {
        // 插入尾部
        while (first_blk) {
            // 不断从first_blk中取块，然后插入到buf的后面
            page_t* next_blk = pktbuf_blk_next(first_blk);

            list_insert_last(&buf->blk_list, &first_blk->node);         // 插入到后面
            buf->total_size += first_blk->size;

            first_blk = next_blk;
        }
    } else {
        page_t *pre = (page_t*)0;

        // 逐个取头部结点依次插入
        while (first_blk) {
            page_t *next = pktbuf_blk_next(first_blk);

            if (pre) {
                list_insert_after(&buf->blk_list, &pre->node, &first_blk->node);
            } else {
                list_insert_first(&buf->blk_list, &first_blk->node);
            }
            buf->total_size += first_blk->size;

            pre = first_blk;
            first_blk = next;
        };
    }
}

/**
 * @brief 分配数据包块
 */
packet_t* pktbuf_alloc(int size) {
    // 分配一个结构
    locker_lock(&locker);
    packet_t* buf = mblock_alloc(&pktbuf_list, -1);
    locker_unlock(&locker);
    if (!buf) {
        log_error(LOG_PACKET_BUFFER, "no pktbuf");
        return (packet_t*)0;
    }

    // 字段值初始化
    buf->total_size = 0;
    init_list(&buf->blk_list);
    list_node_init(&buf->node);

    // 分配块空间
    if (size) {
        page_t* block = pktblock_alloc_list(size, 0);
        if (!block) {
            mblock_free(&pktbuf_list, buf);
            return (packet_t*)0;
        }
        pktbuf_insert_blk_list(buf, block, 1);
    }

    // 检查一下buf的完整性和正确性
    display_check_buf(buf);
    return buf;
}

/**
 * @brief 释放数据包
 */
void pktbuf_free (packet_t * buf) {
}

/**
 * 数据包管理初始化
 */
net_err_t pktbuf_init(void) {
    log_info(LOG_PACKET_BUFFER,"init pktbuf list.");

    // 建立空闲链表. TODO：在嵌入式设备中改成不可共享
    locker_init(&locker, LOCKER_THREAD);
    mblock_init(&block_list, block_buffer, sizeof(page_t), LOG_PACKET_BUFFER, LOG_PACKET_BUFFER);
    mblock_init(&pktbuf_list, pktbuf_buffer, sizeof(packet_t), LOG_PACKET_BUFFER, LOG_PACKET_BUFFER);
    log_info(LOG_PACKET_BUFFER,"init done.");

    return NET_OK;
}
