#include "tcp.h"
#include "tcp_out.h"
#include "tcp_in.h"

/**
 * RFC 793 Page 64
 * in the chapter SEGMENT ARRIVES, there is detailed information about
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

void tcp_time_wait (tcp_t * tcp);

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
    if (seg->hdr->f_rst == 0) {
        log_warning(LOG_TCP, "%s: received segment in closed state abort and reset", tcp ? tcp_state_name(tcp->state) : "unknown");
        tcp_send_reset(seg);
    }
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
        tcp_read_options(tcp, tcp_hdr);
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
    tcp_hdr_t *tcp_hdr = seg->hdr;

    // check RST
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    // check SYN, if set, send a reset and abort the connection
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    // process ack, move the send window variables
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }

    // process data, including FIN, in this function, ACK might be sent
    tcp_data_in(tcp, seg);

    tcp_transmit(tcp);
    if (tcp->flags.fin_in) {
        tcp_set_state(tcp, TCP_STATE_CLOSE_WAIT);
    }
    return NET_OK;
}

/**
 * CLOSE_WAIT
 * passive close state
 * in this state, the remote has already sent a FIN to us
 * we can send data to the remote, but we don't need to receive any data
 */
net_err_t tcp_close_wait_in (tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t *tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  dump ack %d ?", tcp_state_name(tcp->state), seg->hdr->ack);
        return NET_ERR_UNREACH;
    }
    // flush buffer
    tcp_transmit(tcp);
    return NET_OK;
}

/**
 * LAST_ACK
 * in this state, we are the passive close side,
 * and we have already sent a FIN to the remote, waiting a final ACK for our FIN
 * in this state, we don't accept any more data from the remote
 */
net_err_t tcp_last_ack_in (tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    // if receive a SYN, send a reset and abort the connection
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    // process ack
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }

    // TODO: ack might be for data not for FIN, check it and ensure all data has been acked
    if (tcp->flags.fin_out ==0) {
        return tcp_abort(tcp, NET_ERR_CLOSED);
    }
    return NET_OK;
}

/**
 * FIN_WAIT_1
 * we are the active close side,
 * and we have already sent a FIN to the remote
 * we can receive data from the remote, but we don't need to send any data
 */
net_err_t tcp_fin_wait_1_in(tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t *tcp_hdr = seg->hdr;
    // check rst
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    // process ack
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }
    // because in this state, it is half-close, we can still receive data from the remote
    tcp_data_in(tcp, seg);
    tcp_transmit(tcp);
    // checkout tcp_ack_process, if receive ack for FIN, it sets fin_out to 0
    log_info(LOG_TCP, "fin_out %d and tcp_hdr_fin %d", tcp->flags.fin_out, tcp->flags.fin_in);
    if (tcp->flags.fin_out == 0) {
        if (tcp->flags.fin_in) {
            // this is for merged three-way hand wave
            tcp_time_wait(tcp);
        } else {
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_2);
            //sock_wakeup(tcp, SOCK_WAIT_CONN, NET_ERR_OK);
        }
    } else if (tcp->flags.fin_in) {
        // this is for simultaneous close
        tcp_set_state(tcp, TCP_STATE_CLOSING);
    }
    return NET_OK;
}


/**
 * FIN_WAIT_2
 * in this state, we can still receive data from the remote
 * if we receive a FIN from the remote, we will send an ACK
 * and enter the TIME_WAIT state because of the possible retransmission of the ACK
 */
net_err_t tcp_fin_wait_2_in(tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }
    tcp_data_in(tcp, seg);
    if (tcp->flags.fin_in) {
        tcp_time_wait(tcp);
    }
    return NET_OK;
}

/**
 * CLOSING
 * used in simultaneous close
 */
net_err_t tcp_closing_in (tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }
    tcp_transmit(tcp);
    if (tcp->flags.fin_out ==0) {
        tcp_time_wait(tcp);
    }
    return NET_OK;
}


/**
 * TIME_WAIT
 * in this state, we have received a FIN from the remote, and sent an ACK
 */
net_err_t tcp_time_wait_in (tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t *tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }
    tcp_data_in(tcp, seg);
    // send ack for final FIN, reset the timer, ignore other segments
    if (tcp->flags.fin_in) {
        tcp_send_ack(tcp, seg);
        tcp_time_wait(tcp);
    }
    return NET_OK;
}


/**
 * LISTEN state
 * when received unexpected segment, do not abort, send rst and remain in LISTEN
 *
 * */
net_err_t tcp_listen_in(tcp_t *tcp, tcp_seg_t *seg) {
    tcp_hdr_t * tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        return NET_OK;
    }
    if (tcp_hdr->f_ack) {
        log_warning(LOG_TCP, "%s: recieve a ack", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return NET_OK;
    }
    if (tcp_hdr->f_syn) {
        // check the backlog
        if (tcp_backlog_count(tcp) >= tcp->conn.backlog) {
            log_warning(LOG_TCP, "backlog full");
            return NET_ERR_FULL;
        }
        tcp_t * child = tcp_create_child(tcp, seg);
        if (child == (tcp_t *)0) {
            log_warning(LOG_TCP, "error: no tcp for accept");
            return NET_ERR_MEM;
        }
        // ack SYN, enter SYN_RECVD state
        tcp_send_syn(child);
        tcp_set_state(child, TCP_STATE_SYN_RECVD);
        return NET_OK;
    }
    return NET_ERR_STATE;
}



net_err_t tcp_syn_recvd_in(tcp_t *tcp, tcp_seg_t *seg) {
    tcp_hdr_t * tcp_hdr = seg->hdr;
    if (tcp_hdr->f_rst) {
        log_warning(LOG_TCP, "%s: recieve a rst", tcp_state_name(tcp->state));
        // if passive open, abort
        if (tcp->parent) {
            return tcp_abort(tcp, NET_ERR_RESET);
        } else {
            return tcp_abort(tcp, NET_ERR_REFUSED);
        }
    }
    if (tcp_hdr->f_syn) {
        log_warning(LOG_TCP, "%s: recieve a syn", tcp_state_name(tcp->state));
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_ack_process(tcp, seg) < 0) {
        log_warning(LOG_TCP, "%s:  ack process failed", tcp_state_name(tcp->state));
        return NET_ERR_UNREACH;
    }
    // RFC793
    if (tcp_hdr->f_fin) {
        tcp_set_state(tcp, TCP_STATE_CLOSE_WAIT);
        if (tcp->parent) {
            sock_wakeup((sock_t *)tcp->parent, SOCK_WAIT_CONN, NET_ERR_REFUSED);
        }
    } else {
        // enter established and start keepalive
        tcp_set_state(tcp, TCP_STATE_ESTABLISHED);
        tcp_keepalive_start(tcp, tcp->flags.keep_enable);
        if (tcp->parent) {
            sock_wakeup((sock_t *)tcp->parent, SOCK_WAIT_CONN, NET_OK);
        }
    }
    tcp_transmit(tcp);
    return NET_OK;
}



/**
 * free tcb after 2MSL timeout
 */
void tcp_timewait_tmo (struct _net_timer_t* timer, void * arg) {
    tcp_t * tcp = (tcp_t *)arg;
    log_info(LOG_TCP, "tcp free: 2MSL");
    tcp_show_info("tcp free(2MSL)", tcp);
    tcp_free(tcp);
}

/**
 * wait for 2 MSL
 * this is to retransmit ACK for peer
 */
void tcp_time_wait (tcp_t * tcp) {
    tcp_set_state(tcp, TCP_STATE_TIME_WAIT);
    tcp_kill_all_timers(tcp);
    net_timer_add(&tcp->conn.keep_timer, "2msl timer", tcp_timewait_tmo, tcp, 2 * TCP_TMO_MSL, 0);
    sock_wakeup(&tcp->base, SOCK_WAIT_ALL, NET_ERR_CLOSED);
}
