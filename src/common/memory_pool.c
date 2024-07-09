#include "sys_plat.h"
#include "memory_pool.h"
#include "log.h"
#include "list.h"
#include "easy_net_config.h"


net_err_t memory_pool_init (memory_pool_t* mem_pool, void * mem, int blk_size, int cnt, locker_type_t share_type) {
    // The size of each block must be greater than or equal to the size of list_node_t
    assert_halt(blk_size >= sizeof(list_node_t), "size error");

    // Add all the blocks to the free list
    uint8_t* buf = (uint8_t*)mem;
    init_list(&mem_pool->free_list);
    for (int i = 0; i < cnt; i++, buf += blk_size) {
        list_node_t* block = (list_node_t*)buf;
        list_node_init(block);
        list_insert_last(&mem_pool->free_list, block);
    }
    locker_init(&mem_pool->locker, share_type);
    if (share_type != LOCKER_NONE) {
        mem_pool->alloc_sem = sys_sem_create(cnt);
        if (mem_pool->alloc_sem == SYS_SEM_INVALID) {
            log_error(LOG_MEMORY_POOL, "create sem failed.");
            locker_destroy(&mem_pool->locker);
            return NET_ERR_SYS;
        }
    }
    return NET_OK;
}

void * memory_pool_alloc(memory_pool_t* mem_pool, int ms) {
    if ((ms < 0) || (mem_pool->locker.type == LOCKER_NONE)) {
        locker_lock(&mem_pool->locker);
        int count = list_count(&mem_pool->free_list);
        locker_unlock(&mem_pool->locker);
        if (count == 0) {
            return (void*)0;
        }
    }
    if (mem_pool->locker.type != LOCKER_NONE) {
        sys_sem_wait(mem_pool->alloc_sem, ms);
    }
    locker_lock(&mem_pool->locker);
    list_node_t* block = list_remove_first(&mem_pool->free_list);
    locker_unlock(&mem_pool->locker);
    return block;
}

int memory_pool_free_cnt(memory_pool_t* list) {
    locker_lock(&list->locker);
    int count = list_count(&list->free_list);
    locker_unlock(&list->locker);
    return count;
}

void memory_pool_free(memory_pool_t* mem_pool, void* block) {
    locker_lock(&mem_pool->locker);
    list_insert_last(&mem_pool->free_list, (list_node_t *)block);
    locker_unlock(&mem_pool->locker);

    if (mem_pool->locker.type != LOCKER_NONE) {
        sys_sem_notify(mem_pool->alloc_sem);
    }
}


void memory_pool_destroy(memory_pool_t*  mem_pool) {
    if (mem_pool->locker.type != LOCKER_NONE) {
        sys_sem_free(mem_pool->alloc_sem);
        locker_destroy(&mem_pool->locker);
    }
}
