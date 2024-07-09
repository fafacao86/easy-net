#ifndef EASY_NET_NET_ERRORS_H
#define EASY_NET_NET_ERRORS_H

typedef enum _net_err_t {
    NET_OK = 0,    // No error
    NET_ERR_SYS = -1,   // System error
    NET_ERR_FULL = -2,  // Queue is full
    NET_ERR_TIMEOUT = -3, // Timeout
    NET_ERR_MEM = -4,    // Memory allocation error
}net_err_t;

#endif
