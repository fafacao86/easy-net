#include "list.h"
#include "log.h"
#include "timer.h"
#include "sys_plat.h"
#include "easy_net_config.h"


static list_t timer_list;

#if LOG_DISP_ENABLED(LOG_TIMER)
static void display_timer_list(void) {
    plat_printf("--------------- timer list ---------------\n");

    list_node_t* node;
    int index = 0;
    list_for_each(node, &timer_list) {
        net_timer_t* timer = list_entry(node, net_timer_t, node);

        plat_printf("%d: %s, period = %d, curr: %d ms, reload: %d ms\n",
            index++, timer->name,
            timer->flags & NET_TIMER_RELOAD ? 1 : 0,
            timer->curr, timer->reload);
    }
    plat_printf("---------------- timer list end ------------\n");
}
#else
#define display_timer_list()
#endif

/**
 * Think of the timer as Difference Array
 * The expire time of each timer is sum from 0 up tp the its index
 * */
static void insert_timer(net_timer_t * insert) {
    list_node_t* node;
    list_node_t *pre = (list_node_t *)0;

    list_for_each(node, &timer_list) {
        net_timer_t * curr = list_entry(node, net_timer_t, node);

        // new timer is greater than current timer,
        // subtract current timer's expire time from new timer's expire time and move forward
        if (insert->curr > curr->curr) {
            insert->curr -= curr->curr;
        } else if (insert->curr == curr->curr) {
            // new timer is equal to current timer,
            // insert new timer after current timer, and make current timer's expire time 0
            insert->curr = 0;
            list_insert_after(&timer_list, node, &insert->node);
            return;
        } else {
            // new timer is less than current timer,
            // subtract insert timer's expire time from current timer's expire time
            // and insert new timer before current timer
            curr->curr -= insert->curr;
            if (pre) {
                list_insert_after(&timer_list, pre, &insert->node);
            } else {
                list_insert_first(&timer_list, &insert->node);
            }
            return;
        }
        pre = node;
    }

    // if no timer is greater than new timer, insert new timer at the end of list
    list_insert_last(&timer_list, &insert->node);
}


net_err_t net_timer_add(net_timer_t * timer, const char * name, timer_proc_t proc, void * arg, int ms, int flags) {
    log_info(LOG_TIMER, "insert timer: %s", name);

    plat_strncpy(timer->name, name, TIMER_NAME_SIZE);
    timer->name[TIMER_NAME_SIZE - 1] = '\0';
    timer->reload = ms;
    timer->curr = timer->reload;
    timer->proc = proc;
    timer->arg = arg;
    timer->flags = flags;
    insert_timer(timer);

    display_timer_list();
    return NET_OK;
}

net_err_t net_timer_init(void) {
    log_info(LOG_TIMER, "timer init");

    init_list(&timer_list);

    log_info(LOG_TIMER, "init done.");
    return NET_OK;
}

void net_timer_remove (net_timer_t * timer) {
    log_info(LOG_TIMER, "remove timer: %s", timer->name);

    list_node_t * node;
    list_for_each(node, &timer_list) {
        net_timer_t * curr = list_entry(node, net_timer_t, node);
        if (curr != timer) {
            continue;
        }
        // if there is next timer.
        // pass the current timer's expire time to next timer.
        list_node_t * next = list_node_next(node);
        if (next) {
            net_timer_t * next_timer = list_entry(next, net_timer_t, node);
            next_timer->curr += curr->curr;
        }

        list_remove(&timer_list, node);
        break;
    }
    display_timer_list();
}

net_err_t net_timer_check_tmo(int diff_ms) {
    // be careful, do not invoke timer_proc while traverse the list
    // because in the timer_proc, the timer may be modified or removed,
    // so we store the timer to a wait list, and execute them after the traversal
    list_t wait_list;
    init_list(&wait_list);

    list_node_t* node = list_first(&timer_list);
    while (node) {
        list_node_t* next = list_node_next(node);

        // subtract the expiry time from the current timer
        // if the expiry time is greater than 0, the timer is not expired
        net_timer_t* timer = list_entry(node, net_timer_t, node);
        log_info(LOG_TIMER, "timer: %s, diff: %d, curr: %d, reload: %d\n",
                 timer->name, diff_ms, timer->curr, timer->reload);
        if (timer->curr > diff_ms) {
            timer->curr -= diff_ms;
            break;
        }

        diff_ms -= timer->curr;

        timer->curr = 0;
        list_remove(&timer_list, &timer->node);
        list_insert_last(&wait_list, &timer->node);
        node = next;
    }

    while ((node = list_remove_first(&wait_list)) != (list_node_t*)0) {
        net_timer_t* timer = list_entry(node, net_timer_t, node);
        timer->proc(timer, timer->arg);
        if (timer->flags & NET_TIMER_RELOAD) {
            timer->curr = timer->reload;
            insert_timer(timer);
        }
    }

    display_timer_list();
    return NET_OK;
}


int net_timer_first_tmo(void) {
    list_node_t* node = list_first(&timer_list);
    if (node) {
        net_timer_t* timer = list_entry(node, net_timer_t, node);
        return timer->curr;
    }
    return 0;
}
