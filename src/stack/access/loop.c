#include "loop.h"
#include "netif.h"
#include "log.h"
#include "net_errors.h"

net_err_t loop_init(void);

/**
 * open loop back interface
 */
static net_err_t loop_open(netif_t* netif, void* ops_data) {
    netif->type = NETIF_TYPE_LOOP;
    return NET_OK;
}

static void loop_close (struct _netif_t* netif) {
}


static net_err_t loop_transmit (struct _netif_t* netif) {
    // receive packet from the input queue of loop interface
    // and put it into the output queue of loop interface
    packet_t * pktbuf = netif_get_out(netif, -1);
    if (pktbuf) {
        net_err_t err = netif_put_in(netif, pktbuf, -1);
        if (err < 0) {
            log_warning(LOG_NETIF, "netif full");
            packet_free(pktbuf);
            return err;
        }
    }
    return NET_OK;
}

// loop interface driver
static const netif_ops_t loop_driver = {
        .open = loop_open,
        .close = loop_close,
        .transmit = loop_transmit,
};


net_err_t loop_init(void) {
    log_info(LOG_NETIF, "init loop");

    netif_t * netif = netif_open("loop", &loop_driver, (void*)0);
    if (!netif) {
        log_error(LOG_NETIF, "open loop error");
        return NET_ERR_NONE;
    }

    ipaddr_t ip, mask;
    ipaddr_from_str(&ip, "127.0.0.1");
    ipaddr_from_str(&mask, "255.0.0.0");
    netif_set_addr(netif, &ip, &mask, (ipaddr_t*)0);
    netif_set_active(netif);
    log_info(LOG_NETIF, "init done");
    return NET_OK;;
}

