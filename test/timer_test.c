#include "testcase.h"
static void timer0_proc(net_timer_t* timer, void * arg) {
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

static void timer1_proc(net_timer_t* timer, void * arg) {
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

static void timer2_proc(net_timer_t* timer, void * arg) {
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

static void timer3_proc(net_timer_t* timer, void * arg) {
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

void test_timer(){
    static net_timer_t t0, t1, t2, t3;
    // single shot timer
    net_timer_add(&t0, "t0", timer0_proc, (void *)0, 200, 0);
    // auto reload timer
    net_timer_add(&t1, "t1", timer1_proc, (void *)0, 1000, NET_TIMER_RELOAD);
    net_timer_add(&t2, "t2", timer2_proc, (void *)0, 1000, NET_TIMER_RELOAD);
    net_timer_add(&t3, "t3", timer3_proc, (void *)0, 4000, NET_TIMER_RELOAD);
    net_timer_remove(&t1);
}