#include "tcp.h"
#include "tcp_out.h"

/**
 * RFC 793 Page 64
 * int the chapter SEGMENT ARRIVES, there is detailed information about
 * how to process input segment in each state.
 * */


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
 * all data in the incoming segment is discarded.  An incoming
  segment containing a RST is discarded.  An incoming segment not
  containing a RST causes a RST to be sent in response.  The
  acknowledgment and sequence field values are selected to make the
  reset sequence acceptable to the TCP that sent the offending
  segment.
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
    tcp_hdr_t *tcp_hdr = seg->hdr;
    // different from other states, here we don't check the seq, because the connection is not established yet

    // first check ACK bit, if set, check the ack number is acceptable
    if (tcp_hdr->f_ack) {
        // follow RFC 793
        if ((tcp_hdr->ack - tcp->snd.iss <= 0) || (tcp_hdr->ack - tcp->snd.nxt > 0)) {
            log_warning(LOG_TCP, "%s: ack incorrect", tcp_state_name(tcp->state));
            return tcp_send_reset(seg);
        }
    }

    // check RST, and the RST's ACK number must be acceptable
    // if check passed, abort the connection
    if (tcp_hdr->f_rst) {
        if (!tcp_hdr->f_ack) {
            return NET_OK;
        }
        // notify all threads waiting on this socket
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    // check SYN, in this state, we handle only SYN+ACK(reply for our SYN) or SYN(simultaneous open)
    if (tcp_hdr->f_syn) {
        // set recv window variables
        tcp->rcv.iss = tcp_hdr->seq;            // there is IRS in the SYN
        tcp->rcv.nxt = tcp_hdr->seq + 1;        // the received SYN is accounted for one byte
        tcp->flags.irs_valid = 1;               // mark the IRS already set
        if (tcp_hdr->f_ack) {
            tcp_ack_process(tcp, seg);
        }
        if (tcp->snd.una - tcp->snd.iss > 0) {  // this is to check whether we have ack
            // reply an ack for the SYN of peer
            tcp_send_ack(tcp, seg);

            // enter established state
            tcp_set_state(tcp, TCP_STATE_ESTABLISHED);
            sock_wakeup(&tcp->base, SOCK_WAIT_CONN, NET_OK);
        } else {
            // this is for simultaneous open, only SYN is received in syn_sent state
            // this can be tested by first bind a port and then connect, add a breakpoint to debug
            tcp_set_state(tcp, TCP_STATE_SYN_RECVD);

            // send syn+ack to peer
            tcp_send_syn(tcp);
        }
    }
    // ignore other type of segments
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
 * used in simultaneous close
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

