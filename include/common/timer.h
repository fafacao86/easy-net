#ifndef EASY_NET_TIMER_H
#define EASY_NET_TIMER_H

#include "net_errors.h"
#include "list.h"

#define TIMER_NAME_SIZE       32

#define NET_TIMER_RELOAD        (1 << 0)


struct _net_timer_t;
typedef void (*timer_proc_t)(struct _net_timer_t* timer, void * arg);

/**
 * timer
 */
typedef struct _net_timer_t {
    char name[TIMER_NAME_SIZE];
    int flags;      // indicates whether auto reload
    int curr;
    int reload;

    timer_proc_t proc;
    void * arg;
    list_node_t node;
}net_timer_t;

net_err_t net_timer_init(void);
net_err_t net_timer_add(net_timer_t * timer, const char * name, timer_proc_t proc, void * arg, int ms, int flags);
void net_timer_remove (net_timer_t * timer);
net_err_t net_timer_check_tmo(int diff_ms);
int net_timer_first_tmo(void);

#endif //EASY_NET_TIMER_H
