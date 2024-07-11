#ifndef EASY_NET_NET_ERRORS_H
#define EASY_NET_NET_ERRORS_H

typedef enum _net_err_t {
    NET_OK = 0,    // No error
    NET_ERR_SYS = -1,   // System error
    NET_ERR_FULL = -2,  // Queue is full
    NET_ERR_TIMEOUT = -3, // Timeout
    NET_ERR_MEM = -4,    // Memory allocation error
    NET_ERR_SIZE = -5,    // Packet size error
    NET_ERR_PARAM = -6,  // Invalid parameter
    NET_ERR_STATE = -7,  // Invalid state
    NET_ERR_NONE = -8,   // No resource
}net_err_t;

#endif
