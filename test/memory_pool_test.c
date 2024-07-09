#include "testcase.h"

void test_memory_pool(){
    static uint8_t buffer[10][100];
    memory_pool_t mem_pool;

    memory_pool_init(&mem_pool, buffer, 100, 10, LOCKER_THREAD);
    void* temp[10];

    for (int i = 0; i < 10; i++) {
        temp[i] = memory_pool_alloc(&mem_pool, 0);
        printf("block: %p, free count:%d\n", temp[i], memory_pool_free_cnt(&mem_pool));
    }
    for (int i = 0; i < 10; i++) {
        memory_pool_free(&mem_pool, temp[i]);
        printf("free count:%d\n", memory_pool_free_cnt(&mem_pool));
    }

    memory_pool_destroy(&mem_pool);
}
