﻿#include "net_errors.h"
#include "tcp.h"
#include "utils.h"
#include "protocols.h"
#include "ipv4.h"
#include "log.h"

/**
 * because the header length unit is 4 bytes
 * */
void tcp_set_hdr_size (tcp_hdr_t * hdr, int size) {
    hdr->shdr = size / 4;
}

/**
 * here we only set MSS
 * */
static void write_sync_option (tcp_t * tcp, packet_t * buf) {
    int opt_len = sizeof(tcp_opt_mss_t);
    net_err_t err = packet_resize(buf, buf->total_size + opt_len);
    if (err < 0) {
        log_error(LOG_TCP, "resize error");
        return;
    }
    tcp_opt_mss_t mss;
    mss.kind = TCP_OPT_MSS;
    mss.length = sizeof(tcp_opt_mss_t);
    mss.mss = e_ntohs(tcp->mss);
    packet_reset_pos(buf);
    packet_seek(buf, sizeof(tcp_hdr_t));
    packet_write(buf, (uint8_t *)&mss, sizeof(mss));
}


/**
 * convert endianness of tcp header, and calculate checksum
 * then send via ipv4_out
 * */
static net_err_t send_out (tcp_hdr_t * out, packet_t * buf, ipaddr_t * dest, ipaddr_t * src) {
    tcp_display_pkt("tcp out", out, buf);
    out->sport = e_htons(out->sport);
    out->dport = e_htons(out->dport);
    out->seq = e_htonl(out->seq);
    out->ack = e_htonl(out->ack);
    out->win = e_htons(out->win);
    out->urgptr = e_htons(out->urgptr);

    out->checksum = 0;
    out->checksum = checksum_peso(dest->a_addr, src->a_addr, NET_PROTOCOL_TCP, buf);

    net_err_t err = ipv4_out(NET_PROTOCOL_TCP, dest, src, buf);
    if (err < 0) {
        log_info(LOG_TCP, "send tcp buf error");
        packet_free(buf);
    }

    return err;
}

/**
 * about the scenario of sending reset segment:
 * https://www.ibm.com/support/pages/tcpip-sockets-reset-concerns
 * https://stackoverflow.com/questions/251243/what-causes-a-tcp-ip-reset-rst-flag-to-be-sent
 *
 * The rational is: TCP connection is a state machine,
 * if you receive any segment that is not expected in your current state,
 * send a reset segment to inform the other side of the problem.
 */
net_err_t tcp_send_reset(tcp_seg_t * seg) {
    // do not send reset for rst segment, or else it will cause infinite loop
    tcp_hdr_t * in = seg->hdr;
    if (in->f_rst) {
        log_info(LOG_TCP, "reset, ignore");
        return NET_OK;
    }

    // allocate a packet buffer, and fill in the TCP header
    packet_t* buf = packet_alloc(sizeof(tcp_hdr_t));
    if (!buf) {
        log_warning(LOG_TCP, "no pktbuf");
        return NET_ERR_NONE;
    }

    tcp_hdr_t* out = (tcp_hdr_t*)packet_data(buf);
    out->sport = in->dport;
    out->dport = in->sport;
    out->flags = 0;
    out->f_rst = 1;
    log_info(LOG_TCP, "in ack %d", in->f_ack);
    // all settings below is to ensure that the peer will accept the reset segment, instead of dropping it
    if (in->f_ack) {
        // ack is the byte the sender is expecting to receive next
        // if the input segment has ack flag,
        // then set the rst segment's seq to the peer segment's ack
        out->seq = in->ack;
        // in rst we don't need to set ack flag, because we don't need to receive any data
        out->ack = 0;
        out->f_ack = 0;
    } else {
        // if the peer doesn't have ack flag, such as pure SYN segment,
        // then we don't need to set seq field
        out->seq = 0;
        // we need to ack the peer's seq, or else peer will drop this segment
        out->ack = in->seq + seg->seq_len;
        out->f_ack = 1;
    }
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));

    out->win = out->urgptr = 0;
    return send_out(out, buf, &seg->remote_ip, &seg->local_ip);
}


/**
 * calculate the length of data to be sent (not including FIN and SYN)
 */
static void get_send_info (tcp_t * tcp, int * doff, int * dlen) {
    *doff = tcp->snd.nxt - tcp->snd.una;
    *dlen = tcp_buf_cnt(&tcp->snd.buf) - *doff;
    if (*dlen == 0) {
        return;
    }
    // if the data length is greater than the MSS, then set it to the MSS
    *dlen = (*dlen > tcp->mss) ? tcp->mss : *dlen;
}


/**
 * copy data from socket buffer to packet buffer
 */
static int copy_send_data (tcp_t * tcp, packet_t * packet, int doff, int dlen) {
    if (dlen == 0) {
        return 0;
    }
    net_err_t err = packet_resize(packet, (int)(packet->total_size + dlen));
    if (err < 0) {
        log_error(LOG_TCP, "pktbuf resize error");
        return -1;
    }

    int hdr_size = tcp_hdr_size((tcp_hdr_t *)packet_data(packet));
    packet_reset_pos(packet);
    packet_seek(packet, hdr_size);
    tcp_buf_read_send(&tcp->snd.buf, doff, packet, dlen);
    return dlen;
}


/**
 * send an segment with flags specified in socket
 * data to be sent is copied from socket buffer to packet buffer
 */
net_err_t tcp_transmit(tcp_t * tcp) {
    int dlen, doff;
    // calculate the length of data to be sent (not including FIN and SYN)
    // doff is the offset of the first byte to be sent
    // dlen is the length of the data to be sent
    get_send_info(tcp, &doff, &dlen);
    if (dlen < 0) {
        return NET_OK;
    }
    int seq_len = dlen;
    if (tcp->flags.syn_out) {
        seq_len++;
    }
    if ((tcp_buf_cnt(&tcp->snd.buf) == 0) && tcp->flags.fin_out) {
        seq_len++;
    }
    // this is to prevent duplicate empty ACKs, but allow empty FIN and SYN
    if (seq_len == 0) {
        return NET_OK;
    }
    packet_t* buf = packet_alloc(sizeof(tcp_hdr_t));
    if (!buf) {
        log_error(LOG_TCP, "no buffer");
        return NET_OK;
    }
    // set the header of the SYN using the metadata in the socket
    tcp_hdr_t* hdr = (tcp_hdr_t*)packet_data(buf);
    hdr->sport = tcp->base.local_port;
    hdr->dport = tcp->base.remote_port;
    hdr->seq = tcp->snd.nxt;
    hdr->ack = tcp->rcv.nxt;
    hdr->flags = 0;
    hdr->f_syn = tcp->flags.syn_out;
    hdr->f_ack = tcp->flags.irs_valid;
    // if the buffer is not empty, do not send FIN
    if (tcp->flags.fin_out) {
        hdr->f_fin = (tcp_buf_cnt(&tcp->snd.buf) == 0) ? 1 : 0;
    }
    if (hdr->f_syn) {
        write_sync_option(tcp, buf);
    }
    hdr->win = (uint16_t)tcp_rcv_window(tcp);
    hdr->urgptr = 0;
    tcp_set_hdr_size(hdr, buf->total_size);
    copy_send_data(tcp, buf, doff, dlen);
    // move the seq forward
    tcp->snd.nxt += dlen + hdr->f_syn + hdr->f_fin;
    return send_out(hdr, buf, &tcp->base.remote_ip, &tcp->base.local_ip);
}

/**
 * cache the syn_out flag
 */
net_err_t tcp_send_syn(tcp_t* tcp) {
    tcp->flags.syn_out = 1;
    tcp_transmit(tcp);
    return NET_OK;
}


/**
 * seg is an input segment, which has ACK flag set
 * we need to update window variables based on it
 * and clear flags in socket when corresponding ack received
 * */
net_err_t tcp_ack_process (tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t * tcp_hdr = seg->hdr;

    // set the syn_out flag to 0,
    // because we have received the first ACK for the connection
    if (tcp->flags.syn_out) {
        tcp->snd.una++;
        tcp->flags.syn_out = 0;
    }
    // remove data in the buffer that has been acknowledged
    int acked_cnt = tcp_hdr->ack - tcp->snd.una;
    int unacked_cnt = tcp->snd.nxt - tcp->snd.una;
    int curr_acked = (acked_cnt > unacked_cnt) ? unacked_cnt : acked_cnt;
    log_info(LOG_TCP, "curr_acked %d, unacked %d acked %d", curr_acked, unacked_cnt, acked_cnt);
    if (curr_acked > 0) {
        tcp->snd.una += curr_acked;
        curr_acked -= tcp_buf_remove(&tcp->snd.buf, curr_acked);
        // if the ack is for FIN, then clear the fin_out flag
        if (curr_acked && (tcp->flags.fin_out)) {
            tcp->flags.fin_out = 0;
        }
        sock_wakeup(&tcp->base, SOCK_WAIT_WRITE, NET_OK);
    }
    return NET_OK;
}


/**
 * send a pure ACK segment to the peer
 */
net_err_t tcp_send_ack(tcp_t* tcp, tcp_seg_t * seg) {
    // do not ack RST
    if (seg->hdr->f_rst) {
        return NET_OK;
    }
    packet_t* buf = packet_alloc(sizeof(tcp_hdr_t));
    if (!buf) {
        log_error(LOG_TCP, "no buffer");
        return NET_ERR_NONE;
    }
    tcp_hdr_t* out = (tcp_hdr_t*)packet_data(buf);
    out->sport = tcp->base.local_port;
    out->dport = tcp->base.remote_port;
    out->seq = tcp->snd.nxt;
    out->ack = tcp->rcv.nxt;
    out->flags = 0;
    out->f_ack = 1;
    out->urgptr = 0;
    out->win = (uint16_t)tcp_rcv_window(tcp);
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));

    // the connection might have not been established yet,
    // so we use the remote_ip in the seg instead of the remote_ip in the socket
    return send_out(out, buf, &seg->remote_ip, &seg->local_ip);
}


/**
 * send pure FIN
 */
net_err_t tcp_send_fin (tcp_t* tcp) {
    tcp->flags.fin_out = 1;
    tcp_transmit(tcp);
    return NET_OK;
}



int tcp_write_sndbuf(tcp_t * tcp, const uint8_t * buf, int len) {
    int free_cnt = tcp_buf_free_cnt(&tcp->snd.buf);
    if (free_cnt <= 0) {
        // if there is no free space in the send buffer, return 0
        return 0;
    }
    // actual length to write, might be less than len
    int wr_len = (len > free_cnt) ? free_cnt : len;
    tcp_buf_write_send(&tcp->snd.buf, buf, wr_len);
    return wr_len;
}

