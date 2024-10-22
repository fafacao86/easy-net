#include "net_errors.h"
#include "ipaddr.h"
#include "packet_buffer.h"
#include "utils.h"
#include "tcp_in.h"
#include "log.h"
#include "protocols.h"
#include "tcp_out.h"
#include "tcp_state.h"

int tcp_hdr_size (tcp_hdr_t * hdr) {
    return hdr->shdr * 4;
}



void tcp_seg_init (tcp_seg_t * seg, packet_t * buf, ipaddr_t * local, ipaddr_t * remote) {
    seg->buf = buf;
    seg->hdr = (tcp_hdr_t*)packet_data(buf);

    ipaddr_copy(&seg->local_ip, local);
    ipaddr_copy(&seg->remote_ip, remote);
    seg->data_len = buf->total_size - tcp_hdr_size(seg->hdr);
    seg->seq = seg->hdr->seq;
    // in terms of seq, the SYN and FIN will both take up one position
    // SYN + data + FIN
    seg->seq_len = seg->data_len + seg->hdr->f_syn + seg->hdr->f_fin;
}


/**
 * RFC 793:
 * Segment Receive  Test
 * Length  Window
 * ------- -------  -------------------------------------------
 *    0       0     SEG.SEQ = RCV.NXT
 *    0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
 *   >0       0     not acceptable
 *   >0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
 *                  or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND
 * */
static int tcp_seq_acceptable(tcp_t *tcp, tcp_seg_t *seg) {
    uint32_t rcv_win = tcp_rcv_window(tcp);

    if (seg->seq_len == 0) {
        if (rcv_win == 0) {
            // 0(len)   0(win)     SEG.SEQ = RCV.NXT
            return seg->seq == tcp->rcv.nxt;
        } else {
            // window is not empty, if seq is within the window, it's acceptable
            // 0(len)   >0(win)     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
            int v = TCP_SEQ_LE(tcp->rcv.nxt, seg->seq) && TCP_SEQ_LE(seg->seq, tcp->rcv.nxt + rcv_win - 1);
            return v;
        }
    } else {
        if (rcv_win == 0) {
            return 0;
        } else {
            // as long as there is overlap with the window, it's acceptable
            // which is head is within the window, or tail is within the window
            uint32_t slast = seg->seq + seg->seq_len - 1;       // slast is the tail
            int v = TCP_SEQ_LE(tcp->rcv.nxt, seg->seq) && TCP_SEQ_LE(seg->seq, tcp->rcv.nxt + rcv_win - 1);
            //int v = (seg->seq - tcp->rcv.nxt >= 0) && ((seg->seq - (tcp->rcv.nxt + rcv_win)) < 0);
            v |= TCP_SEQ_LE(tcp->rcv.nxt, slast) && TCP_SEQ_LE(slast, tcp->rcv.nxt + rcv_win - 1);
            // v |= (slast - tcp->rcv.nxt >= 0) && ((slast - (tcp->rcv.nxt + rcv_win)) < 0);
            return v;
        }
    }
}



/**
 * handle a TCP packet
 * firstly check seq, if it is acceptable, then process the segment according to the state
 */
net_err_t tcp_in(packet_t *buf, ipaddr_t *src_ip, ipaddr_t *dest_ip) {
    static const tcp_proc_t tcp_state_proc[] = {
            [TCP_STATE_CLOSED] = tcp_closed_in,
            [TCP_STATE_SYN_SENT] = tcp_syn_sent_in,
            [TCP_STATE_ESTABLISHED] = tcp_established_in,
            [TCP_STATE_FIN_WAIT_1] = tcp_fin_wait_1_in,
            [TCP_STATE_FIN_WAIT_2] = tcp_fin_wait_2_in,
            [TCP_STATE_CLOSING] = tcp_closing_in,
            [TCP_STATE_TIME_WAIT] = tcp_time_wait_in,
            [TCP_STATE_CLOSE_WAIT] = tcp_close_wait_in,
            [TCP_STATE_LAST_ACK] = tcp_last_ack_in,
            [TCP_STATE_LISTEN] = tcp_listen_in,
            [TCP_STATE_SYN_RECVD] = tcp_syn_recvd_in,
    };
    tcp_hdr_t * tcp_hdr = (tcp_hdr_t *)packet_data(buf);
    if (packet_set_cont(buf, sizeof(tcp_hdr_t)) < 0) {
        log_error(LOG_TCP, "set cont failed.");
        return -1;
    }
    tcp_hdr = (tcp_hdr_t *)packet_data(buf);
    if (tcp_hdr->checksum) {
        packet_reset_pos(buf);
        if (checksum_peso(dest_ip->a_addr, src_ip->a_addr, NET_PROTOCOL_TCP, buf)) {
            log_warning(LOG_TCP, "tcp checksum incorrect");
            return NET_ERR_BROKEN;
        }
    }
    if ((buf->total_size < sizeof(tcp_hdr_t)) || (buf->total_size < tcp_hdr_size(tcp_hdr))) {
        log_warning(LOG_TCP, "tcp packet size incorrect: %d!", buf->total_size);
        return NET_ERR_SIZE;
    }
    // port has to be non-zero
    if (!tcp_hdr->sport || !tcp_hdr->dport) {
        log_warning(LOG_TCP, "port == 0");
        return NET_ERR_UNREACH;
    }
    // in normal tcp communication, the flags should not be 0
    if (tcp_hdr->flags == 0) {
        log_warning(LOG_TCP, "flag == 0");
        return NET_ERR_UNREACH;
    }
    // careful about endian
    tcp_hdr->sport = e_ntohs(tcp_hdr->sport);
    tcp_hdr->dport = e_ntohs(tcp_hdr->dport);
    tcp_hdr->seq = e_ntohl(tcp_hdr->seq);
    tcp_hdr->ack = e_ntohl(tcp_hdr->ack);
    tcp_hdr->win = e_ntohs(tcp_hdr->win);
    tcp_hdr->urgptr = e_ntohs(tcp_hdr->urgptr);
    tcp_display_pkt("tcp packet in!", tcp_hdr, buf);
    tcp_seg_t seg;
    tcp_seg_init(&seg, buf, dest_ip, src_ip);
    tcp_t *tcp = (tcp_t *)tcp_find(dest_ip, tcp_hdr->dport, src_ip, tcp_hdr->sport);
    if (!tcp || (tcp->state >= TCP_STATE_MAX)) {
        log_error(LOG_TCP, "no tcp found: port = %d", tcp_hdr->dport);
        tcp_send_reset(&seg);
        packet_free(buf);
        tcp_show_list();
        return NET_OK;
    }
    net_err_t err = packet_seek(buf, tcp_hdr_size(tcp_hdr));
    if (err < 0) {
        log_error(LOG_TCP, "seek failed.");
        return NET_ERR_SIZE;
    }
    // these states are the first time to receive data from remote, so no need to check seq
    if ((tcp->state != TCP_STATE_CLOSED)  && (tcp->state != TCP_STATE_SYN_SENT) && (tcp->state != TCP_STATE_LISTEN)) {
        if (!tcp_seq_acceptable(tcp, &seg)) {
            log_info(LOG_TCP, "seq incorrect: %d < %d", seg.seq, tcp->rcv.nxt);
            goto seg_drop;
        }
    }
    tcp_keepalive_restart(tcp);
    tcp_state_proc[tcp->state](tcp, &seg);
    tcp_show_info("after tcp in", tcp);
seg_drop:
    packet_free(buf);
    return NET_OK;
}


/**
 * copy data to receive buffer
 * here we need to handle the case of out-of-order segment and hole and retransmit
 * */
static int copy_data_to_rcvbuf(tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t * tcp_hdr = seg->hdr;
    packet_t * buf = seg->buf;
    // TODO: support hole and out-of-order segment
    int doffset = seg->seq - tcp->rcv.nxt;
    if (seg->data_len && (doffset == 0)) {
        // copy data to rcv buffer, we do not support hole yet
        // tcp_buf_write_rcv() will truncate the data if it is too long
        return tcp_buf_write_rcv(&tcp->rcv.buf, doffset, buf, seg->data_len);
    }
    return 0;
}



/**
 * process the tcp segment input data, extract data to receive buffer
 * if there is FIN, send ACK
 */
net_err_t tcp_data_in (tcp_t * tcp, tcp_seg_t * seg) {
    int size = copy_data_to_rcvbuf(tcp, seg);
    if (size < 0) {
        log_error(LOG_TCP, "copy data to tcp rcvbuf failed.");
        return NET_ERR_SIZE;
    }
    log_info(LOG_TCP, "tcp data in: %d bytes", size);
    // wake up or not
    int wakeup = 0;
    if (size) {
        tcp->rcv.nxt += size ;
        wakeup++;
    }
    tcp_hdr_t * tcp_hdr = seg->hdr;
    // rcv.next == fin_seg.seq means the recv window is empty, all data has been received
    if (tcp_hdr->f_fin && (tcp->rcv.nxt == seg->seq)) {
        tcp->rcv.nxt++;
        tcp->flags.fin_in = 1;
        wakeup++;
    }
    // if there is data, notify the application
    if (wakeup) {
        if (tcp->flags.fin_in) {
            sock_wakeup((sock_t *)tcp, SOCK_WAIT_ALL, NET_ERR_CLOSED);
        } else {
            sock_wakeup((sock_t *)tcp, SOCK_WAIT_READ, NET_OK);
        }
        // TODO：delay ACK
        tcp_send_ack(tcp, seg);
    }
    return NET_OK;
}



void tcp_read_options(tcp_t *tcp, tcp_hdr_t * tcp_hdr) {
    uint8_t *opt_start = (uint8_t *)tcp_hdr + sizeof(tcp_hdr_t);
    uint8_t *opt_end = opt_start + (tcp_hdr_size(tcp_hdr) - sizeof(tcp_hdr_t));
    if (opt_end <= opt_start){
        return;
    }
    while (opt_start <= opt_end) {
        tcp_opt_mss_t * opt = (tcp_opt_mss_t *)opt_start;

        switch (opt_start[0]) {
            case TCP_OPT_MSS: {
                if (opt->length == 4) {
                    uint16_t mss = e_ntohs(opt->mss);
                    if (tcp->mss > mss) {
                        tcp->mss = mss;
                    }
                }else {
                    opt_start++;
                }
                opt_start += opt->length;
                break;
            }
            case TCP_OPT_NOP: {
                opt_start++;
                break;
            }
            case TCP_OPT_END: {
                return;
            }
            default: {
                opt_start++;
                break;
            }
        }
    }
}