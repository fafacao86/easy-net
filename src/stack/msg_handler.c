/**
 * This is the message handler for the low level message exchange
 * between the link layer and the upper layers.
 *
 * Threads responsible for the link layer will firstly allocate a block from the memory pool,
 * then put the pointer to the block into the message queue.
 * The message handler thread will then dequeue the message from the message queue, and process it.
 *
*/

#include "net_plat.h"
#include "msg_handler.h"
#include "fixed_queue.h"
#include "easy_net_config.h"
#include "memory_pool.h"
#include "log.h"
#include "netif.h"
#include "sys_plat.h"
#include "timer.h"
#include "ipv4.h"

static void * msg_tbl[HANDLER_BUFFER_SIZE];  // For the message queue
static fixed_queue_t msg_queue;            // message queue
//Be careful: size of exmsg_t must be the greater than list_node_t
static exmsg_t msg_buffer[HANDLER_BUFFER_SIZE];  // For the memory pool
static memory_pool_t msg_mem_pool;          // memory pool


/**
 * API stub, called by consumer thread
 * */
net_err_t exmsg_func_exec(exmsg_func_t func, void * param) {
    // wrap the func pointer and parameter into a func_msg_t struct
    // put it into the handler thread queue
    // then wait on the semaphore for the worker thread to finish the execution
    func_msg_t func_msg;
    func_msg.thread = sys_thread_self();
    func_msg.func = func;
    func_msg.param = param;
    func_msg.err = NET_OK;
    func_msg.wait_sem = sys_sem_create(0);
    if (func_msg.wait_sem == SYS_SEM_INVALID) {
        log_error(LOG_HANDLER, "error create wait sem");
        return NET_ERR_MEM;
    }

    exmsg_t* msg = (exmsg_t*)memory_pool_alloc(&msg_mem_pool, 0);
    msg->type = NET_EXMSG_FUN;
    msg->func_msg = &func_msg;

    log_info(LOG_HANDLER, "1.begin call func: %p", func);
    net_err_t err = fixed_queue_send(&msg_queue, msg, 0);
    if (err < 0) {
        log_error(LOG_HANDLER, "send msg to queue ailed. err = %d", err);
        memory_pool_free(&msg_mem_pool, msg);
        sys_sem_free(func_msg.wait_sem);
        return err;
    }

    sys_sem_wait(func_msg.wait_sem, 0);
    log_info(LOG_HANDLER, "4.end call func: %p", func);

    // free the wait semaphore
    sys_sem_free(func_msg.wait_sem);
    return func_msg.err;
}

/**
 * called by worker thread to execute the function and notify the API consumer thread
 */
static net_err_t do_func(func_msg_t* func_msg) {
    log_info(LOG_HANDLER, "2.calling func");
    func_msg->err = func_msg->func(func_msg);
    sys_sem_notify(func_msg->wait_sem);
    log_info(LOG_HANDLER, "3.func exec complete");
    return NET_OK;
}

/**
 * Initialize the message handler thread
 */
net_err_t init_msg_handler (void) {
    log_info(LOG_HANDLER, "message handler init");
    net_err_t err = fixed_queue_init(&msg_queue, msg_tbl, HANDLER_BUFFER_SIZE, HANDLER_LOCK_TYPE);
    if (err < 0) {
        log_error(LOG_HANDLER, "fixed queue init error");
        return err;
    }

    err = memory_pool_init(&msg_mem_pool, msg_buffer, sizeof(exmsg_t), HANDLER_BUFFER_SIZE, HANDLER_LOCK_TYPE);
    if (err < 0) {
        log_error(LOG_HANDLER,  "memory pool init error");
        return err;
    }
    log_info(LOG_HANDLER, "handler init done.");
    return NET_OK;
}

static net_err_t do_netif_in(exmsg_t* msg) {
    netif_t* netif = msg->netif.netif;

    packet_t* packet;
    while ((packet = netif_get_in(netif, -1))) {
        log_info(LOG_HANDLER, "recv a packet");

        net_err_t err;
        if (netif->link_layer) {
            err = netif->link_layer->in(netif, packet);
            if (err < 0) {
                packet_free(packet);
                log_warning(LOG_HANDLER, "netif in failed. err=%d", err);
            }
        } else {
            // If there is no link layer driver bound to the netif,
            // we assume that it is sent from the loop back netif.
            err = ipv4_in(netif, packet);
            if (err < 0) {
                packet_free(packet);
                log_warning(LOG_HANDLER, "netif in failed. err=%d", err);
            };
        }
    }

    return NET_OK;
}


/**
 * Body of the message handler thread
 */
static void work_thread (void * arg) {
    log_info(LOG_HANDLER, "handler thread is running....\n");
    net_time_t time;
    sys_time_curr(&time);
    int time_last = TIMER_SCAN_PERIOD;
    while (1) {
        int first_tmo = net_timer_first_tmo();
        exmsg_t* msg = (exmsg_t*)fixed_queue_recv(&msg_queue, first_tmo);
        if (msg) {
            log_info(LOG_HANDLER, "recieve a msg(%p): %d", msg, msg->type);
            switch (msg->type) {
                case NET_EXMSG_NETIF_IN:
                    do_netif_in(msg);
                    break;
                case NET_EXMSG_FUN:               // API call
                    do_func(msg->func_msg);
                    break;
            }
            memory_pool_free(&msg_mem_pool, msg);
        }
        int diff_ms = sys_time_goes(&time);
        time_last -= diff_ms;
        if (time_last < 0) {
            net_timer_check_tmo(diff_ms);
            time_last = TIMER_SCAN_PERIOD;
        }
    }

}


/**
 * Start the message handler thread
 */
net_err_t start_msg_handler (void) {
    sys_thread_t thread = sys_thread_create(work_thread, (void *)0);
    if (thread == SYS_THREAD_INVALID) {
        return NET_ERR_SYS;
    }
    return NET_OK;
}

/**
 * This function will be called by the network interface driver
 * to send a message to the message handler thread.
 * The main purpose is to notify the message handler thread to process packets in the msg_queue
 */
net_err_t handler_netif_in(netif_t* netif) {
    exmsg_t* msg = memory_pool_alloc(&msg_mem_pool, -1);
    if (!msg) {
        log_warning(LOG_HANDLER, "no free block");
        return NET_ERR_MEM;
    }
    msg->type = NET_EXMSG_NETIF_IN;
    msg->netif.netif = netif;
    net_err_t err = fixed_queue_send(&msg_queue, msg, -1);
    if (err < 0) {
        log_warning(LOG_HANDLER, "fixed queue full");
        memory_pool_free(&msg_mem_pool, msg);
        return err;
    }
    return NET_OK;
}


net_err_t test_func (struct _func_msg_t* msg) {
    msg->err = 0x1234;
    printf("hello: %x\n", *(int *)msg->param);
    return NET_OK;
}
