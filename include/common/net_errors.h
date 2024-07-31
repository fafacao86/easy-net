#ifndef EASY_NET_NET_ERRORS_H
#define EASY_NET_NET_ERRORS_H

typedef enum _net_err_t {
    NET_ERR_NEED_WAIT = 1, // Need to wait for resource
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
    NET_ERR_WRONG_SOCKET = -14, // Wrong socket fd
    NET_ERR_CONNECTED = -15, // Already connected
    NET_ERR_BINED = -16, // Already binded
    NET_ERR_RESET = -17, // Connection reset by peer
    NET_ERR_CLOSED = -18, // Connection closed by peer
    NET_ERR_ADDR = -19, // Invalid address
    NET_ERR_REFUSED = -20, // Connection refused
}net_err_t;

#endif
