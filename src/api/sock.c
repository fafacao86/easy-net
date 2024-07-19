#include "sys_plat.h"
#include "socket.h"
#include "sock.h"

#define SOCKET_MAX_NR		10
static x_socket_t socket_tbl[SOCKET_MAX_NR];

static inline int get_index(x_socket_t* socket) {
    return (int)(socket - socket_tbl);
}

static inline x_socket_t* get_socket(int idx) {
    if ((idx < 0) || (idx >= SOCKET_MAX_NR)) {
        return (x_socket_t*)0;
    }

    x_socket_t* s = socket_tbl + idx;
    return s;
}

static x_socket_t * socket_alloc (void) {
    x_socket_t * s = (x_socket_t *)0;
    // scan from 0 to SOCKET_MAX_NR to find a free socket
    // like file descriptor allocation in Linux
    for (int i = 0; i < SOCKET_MAX_NR; i++) {
        x_socket_t * curr = socket_tbl + i;
        if (curr->state == SOCKET_STATE_FREE) {
            s = curr;
            s->state = SOCKET_STATE_USED;
            break;
        }
    }
    return s;
}


static void socket_free(x_socket_t* s) {
    s->state = SOCKET_STATE_FREE;
}

net_err_t socket_init(void) {
    plat_memset(socket_tbl, 0, sizeof(socket_tbl));
    return NET_OK;
}
