#ifndef EASY_NET_MEMORY_POOL_H
#define EASY_NET_MEMORY_POOL_H

#include "net_errors.h"
#include "sys_plat.h"
#include "locker.h"
#include "list.h"

/**
 * Memory pool, all blocks are managed by a list, and each block is fixed-size.
 * The purpose is to be portable, some platforms may not support dynamic memory allocation.
 */
typedef struct mem_t{
    void* start;
    list_t free_list;
    locker_t locker;                    // Used to protect the list from race conditions
    sys_sem_t alloc_sem;                // Used to pause threads when all blocks are allocated
}memory_pool_t;

net_err_t memory_pool_init (memory_pool_t* mem_pool, void * mem, int blk_size, int cnt, locker_type_t share_type);

void * memory_pool_alloc(memory_pool_t * mem_pool, int ms);

int memory_pool_free_cnt(memory_pool_t* list);

void memory_pool_free(memory_pool_t * mem_pool, void * block);

void memory_pool_destroy(memory_pool_t* mem_pool);
#endif //EASY_NET_MEMORY_POOL_H
