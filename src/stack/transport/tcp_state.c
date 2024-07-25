#include "tcp.h"

/**
 * get TCP state name by enum value
 */
const char * tcp_state_name (tcp_state_t state) {
    static const char * state_name[] = {
            [TCP_STATE_FREE] = "FREE",
            [TCP_STATE_CLOSED] = "CLOSED",
            [TCP_STATE_LISTEN] = "LISTEN",
            [TCP_STATE_SYN_SENT] = "SYN_SENT",
            [TCP_STATE_SYN_RECVD] = "SYN_RCVD",
            [TCP_STATE_ESTABLISHED] = "ESTABLISHED",
            [TCP_STATE_FIN_WAIT_1] = "FIN_WAIT_1",
            [TCP_STATE_FIN_WAIT_2] = "FIN_WAIT_2",
            [TCP_STATE_CLOSING] = "CLOSING",
            [TCP_STATE_TIME_WAIT] = "TIME_WAIT",
            [TCP_STATE_CLOSE_WAIT] = "CLOSE_WAIT",
            [TCP_STATE_LAST_ACK] = "LAST_ACK",

            [TCP_STATE_MAX] = "UNKNOWN",
    };

    if (state >= TCP_STATE_MAX) {
        state = TCP_STATE_MAX;
    }
    return state_name[state];
}

/**
 * set TCP state and log it
 */
void tcp_set_state (tcp_t * tcp, tcp_state_t state) {
    log_info(LOG_TCP, "TCP state %s -> %s", tcp_state_name(tcp->state), tcp_state_name(state));
    tcp->state = state;
}


/**
 * CLOSED
 * receive a segment in CLOSED state
 * at this state, any segment is discarded, and a RST is sent to the remote
 */
net_err_t tcp_closed_in(tcp_t *tcp, tcp_seg_t *seg) {
    return NET_OK;
}

/**
 * SYN_SENT
 * receive a segment after SYN has been sent
 * normal segment: SYN+ACK, or only SYN (simultaneous open)
 * the other segments are discarded and a RST is sent to the remote *
 * user invoke connect() to enter this state
 */
net_err_t tcp_syn_sent_in(tcp_t *tcp, tcp_seg_t *seg) {
    return NET_OK;
}

/**
 * ESTABLISHED
 * connection established state
 * send and receive data normally
 */
net_err_t tcp_established_in(tcp_t *tcp, tcp_seg_t *seg) {
    return NET_OK;
}

/**
 * CLOSE_WAIT
 * passive close state
 * in this state, the remote has already sent a FIN to us
 * we can send data to the remote, but we don't need to receive any data
 */
net_err_t tcp_close_wait_in (tcp_t * tcp, tcp_seg_t * seg) {
    return NET_OK;
}

/**
 * LAST_ACK
 * in this state, we are the passive close side,
 * and we have already sent a FIN to the remote, waiting a final ACK for our FIN
 * in this state, we don't accept any more data from the remote
 */
net_err_t tcp_last_ack_in (tcp_t * tcp, tcp_seg_t * seg) {
    return NET_OK;
}

/**
 * FIN_WAIT_1
 * we are the active close side,
 * and we have already sent a FIN to the remote
 * we can receive data from the remote, but we don't need to send any data
 */
net_err_t tcp_fin_wait_1_in(tcp_t * tcp, tcp_seg_t * seg) {
    return NET_OK;
}

/**
 * FIN_WAIT_2
 * in this state, we can still receive data from the remote
 * if we receive a FIN from the remote, we will send an ACK
 * and enter the TIME_WAIT state because of the possible retransmission of the ACK
 */
net_err_t tcp_fin_wait_2_in(tcp_t * tcp, tcp_seg_t * seg) {
    return NET_OK;
}

/**
 * CLOSING
 */
net_err_t tcp_closing_in (tcp_t * tcp, tcp_seg_t * seg) {
    return NET_OK;
}

/**
 * TIME_WAIT
 * in this state, we have received a FIN from the remote, and sent an ACK
 */
net_err_t tcp_time_wait_in (tcp_t * tcp, tcp_seg_t * seg) {
    return NET_OK;
}

