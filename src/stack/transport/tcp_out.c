#include "net_errors.h"
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
 * send an empty segment with flags specified in socket
 */
net_err_t tcp_transmit(tcp_t * tcp) {
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
    hdr->f_fin = tcp->flags.fin_out;
    hdr->win = 1024;
    hdr->urgptr = 0;
    tcp_set_hdr_size(hdr, buf->total_size);
    // move the seq forward
    tcp->snd.nxt += hdr->f_syn + hdr->f_fin;
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
 * */
net_err_t tcp_ack_process (tcp_t * tcp, tcp_seg_t * seg) {
    tcp_hdr_t * tcp_hdr = seg->hdr;

    // set the syn_out flag to 0,
    // because we have received the first ACK for the connection
    if (tcp->flags.syn_out) {
        tcp->snd.una++;
        tcp->flags.syn_out = 0;
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
    out->win = 0;
    out->urgptr = 0;
    out->win = 1024;
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