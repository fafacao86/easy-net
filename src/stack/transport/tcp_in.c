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
 * handle a TCP packet
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
    };
    tcp_hdr_t * tcp_hdr = (tcp_hdr_t *)packet_data(buf);
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
    tcp_state_proc[tcp->state](tcp, &seg);
    tcp_show_info("after tcp in", tcp);
    return NET_OK;
}




/**
 * process the tcp segment input data, extract data to receive buffer
 * if there is FIN, send ACK
 */
net_err_t tcp_data_in (tcp_t * tcp, tcp_seg_t * seg) {
    // wake up or not
    int wakeup = 0;

    // TODO: check before ack FIN
    tcp_hdr_t * tcp_hdr = seg->hdr;
    if (tcp_hdr->f_fin) {
        tcp->rcv.nxt++;
        wakeup++;
    }

    // if there is data, notify the application
    if (wakeup) {
        if (tcp_hdr->f_fin) {
            sock_wakeup((sock_t *)tcp, SOCK_WAIT_ALL, NET_ERR_CLOSED);
        } else {
            sock_wakeup((sock_t *)tcp, SOCK_WAIT_READ, NET_OK);
        }

        // TODO：delay ACK
        tcp_send_ack(tcp, seg);
    }

    // 还要给对方发送响应
    return NET_OK;
}