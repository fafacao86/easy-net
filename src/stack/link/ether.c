#include <string.h>
#include "netif.h"
#include "ether.h"
#include "log.h"


static net_err_t ether_open(netif_t* netif) {
    return NET_OK;
}

static void ether_close(netif_t* netif) {

}


static net_err_t ether_in(netif_t* netif, packet_t* buf) {
    return NET_OK;
}

/**
 * register ethernet link layer interface
 * */
net_err_t ether_init(void) {
    static const link_layer_t link_layer = {
            .type = NETIF_TYPE_ETHER,
            .open = ether_open,
            .close = ether_close,
            .in = ether_in,
            // .out = ether_out,
    };

    log_info(LOG_ETHER, "init ether");
    // register ethernet link layer interface
    net_err_t err = netif_register_layer(NETIF_TYPE_ETHER, &link_layer);
    if (err < 0) {
        log_info(LOG_ETHER, "error = %d", err);
        return err;
    }

    log_info(LOG_ETHER, "done.");
    return NET_OK;
}
