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

static void * msg_tbl[HANDLER_BUFFER_SIZE];  // For the message queue
static fixed_queue_t msg_queue;            // message queue
//Be careful: size of exmsg_t must be the greater than list_node_t
static exmsg_t msg_buffer[HANDLER_BUFFER_SIZE];  // For the memory pool
static memory_pool_t msg_mem_pool;          // memory pool



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

        packet_fill(packet, 0x88, 6);
        net_err_t err = netif_out(netif, NULL, packet);
        if(err < 0) {
            packet_free(packet);
        }
    }

    return NET_OK;
}


/**
 * Body of the message handler thread
 */
static void work_thread (void * arg) {
    log_info(LOG_HANDLER, "handler thread is running....\n");
    while (1) {
        exmsg_t* msg = (exmsg_t*)fixed_queue_recv(&msg_queue, 0);
        if (msg == (exmsg_t*)0) {
            continue;
        }

        log_info(LOG_HANDLER, "recieve a msg(%p): %d", msg, msg->type);
        switch (msg->type) {
            case NET_EXMSG_NETIF_IN:
                do_netif_in(msg);
                break;
        }

        memory_pool_free(&msg_mem_pool, msg);
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