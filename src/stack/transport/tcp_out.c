#include "net_errors.h"
#include "tcp.h"
#include "utils.h"
#include "protocols.h"
#include "ipv4.h"
#include "log.h"

/**
 * because the header length unit is 4 bytes
 * */
static inline void tcp_set_hdr_size (tcp_hdr_t * hdr, int size) {
    hdr->shdr = size / 4;
}

static net_err_t send_out (tcp_hdr_t * out, packet_t * buf, ipaddr_t * dest, ipaddr_t * src) {
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
