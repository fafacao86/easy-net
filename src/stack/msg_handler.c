/**
 * This is the message handler for the low level message exchange
 * between the link layer and the upper layers.
*/

#include "net_plat.h"
#include "msg_handler.h"

/**
 * Initialize the message handler thread
 */
net_err_t init_msg_handler (void) {
    return NET_OK;
}

/**
 * Body of the message handler thread
 */
static void work_thread (void * arg) {
    plat_printf("handler thread is running....\n");
    while (1) {
        sys_sleep(1);
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