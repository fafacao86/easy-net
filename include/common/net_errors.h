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
    NET_ERR_IO = -9,     // I/O device error
    NET_ERR_EXIST = -10, // Resource already exists
    NET_ERR_NOT_SUPPORT = -11, // Not supported
    NET_ERR_BROKEN = -12, // Broken packet
    NET_ERR_UNREACH = -13, // Unreachable address
}net_err_t;

#endif
