#include "net_errors.h"
#include "ipaddr.h"
#include "packet_buffer.h"
#include "utils.h"
#include "tcp_in.h"
#include "log.h"
#include "protocols.h"
#include "tcp_out.h"

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
    tcp_send_reset(&seg);
    return NET_OK;
}

