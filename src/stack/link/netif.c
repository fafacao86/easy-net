#include "netif.h"
#include "memory_pool.h"
#include "msg_handler.h"
#include "log.h"

static netif_t netif_buffer[NETIF_DEV_CNT];     // number of network interfaces
static memory_pool_t netif_pool;                   // memory pool for netif
static list_t netif_list;               // list of network interfaces
static netif_t * netif_default;          // default network interface
static const link_layer_t* link_layers[NETIF_TYPE_SIZE];


#if LOG_DISP_ENABLED(DBG_NETIF)
void display_netif_list (void) {
    list_node_t * node;

    plat_printf("netif list:\n");
    list_for_each(node, &netif_list) {
        netif_t * netif = list_entry(node, netif_t, node);
        plat_printf("%s:", netif->name);
        switch (netif->state) {
            case NETIF_CLOSED:
                plat_printf(" %s ", "closed");
                break;
            case NETIF_OPENED:
                plat_printf(" %s ", "opened");
                break;
            case NETIF_ACTIVE:
                plat_printf(" %s ", "active");
                break;
            default:
                break;
        }
        switch (netif->type) {
            case NETIF_TYPE_ETHER:
                plat_printf(" %s ", "ether");
                break;
            case NETIF_TYPE_LOOP:
                plat_printf(" %s ", "loop");
                break;
            default:
                break;
        }
        plat_printf(" mtu=%d ", netif->mtu);
        plat_printf("\n");
        dump_mac("\tmac:", netif->hwaddr.addr);
        dump_ip_buf(" ip:", netif->ipaddr.a_addr);
        dump_ip_buf(" netmask:", netif->netmask.a_addr);
        dump_ip_buf(" gateway:", netif->gateway.a_addr);

        plat_printf("\n");
    }
}
#else
#define display_netif_list()
#endif // DBG_NETIF

net_err_t netif_register_layer(int type, const link_layer_t* layer) {
    if ((type < 0) || (type >= NETIF_TYPE_SIZE)) {
        log_error(LOG_NETIF, "type error: %d", type);
        return NET_ERR_PARAM;
    }
    if (link_layers[type]) {
        log_error(LOG_NETIF, "link layer: %d exist", type);
        return NET_ERR_EXIST;
    }
    link_layers[type] = layer;
    return NET_OK;
}

static const link_layer_t * netif_get_layer(int type) {
    if ((type < 0) || (type >= NETIF_TYPE_SIZE)) {
        return (const link_layer_t*)0;
    }
    return link_layers[type];
}


net_err_t netif_init(void) {
    log_info(LOG_NETIF, "init netif");

    init_list(&netif_list);
    memory_pool_init(&netif_pool, netif_buffer, sizeof(netif_t), NETIF_DEV_CNT, LOCKER_NONE);
    netif_default = (netif_t *)0;
    plat_memset((void *)link_layers, 0, sizeof(link_layers));
    log_info(LOG_NETIF, "netif init done.\n");
    return NET_OK;
}


netif_t* netif_open(const char* dev_name, const netif_ops_t* ops, void * ops_data) {
    netif_t*  netif = (netif_t *)memory_pool_alloc(&netif_pool, -1);
    if (!netif) {
        log_error(LOG_NETIF, "no netif");
        return (netif_t*)0;
    }

    ipaddr_set_any(&netif->ipaddr);
    ipaddr_set_any(&netif->netmask);
    ipaddr_set_any(&netif->gateway);
    netif->mtu = 0;
    netif->type = NETIF_TYPE_NONE;
    list_node_init(&netif->node);

    plat_strncpy(netif->name, dev_name, NETIF_NAME_SIZE);
    netif->name[NETIF_NAME_SIZE - 1] = '\0';
    netif->ops = ops;
    netif->ops_data = (void *)ops_data;

    net_err_t err = fixed_queue_init(&netif->in_q, netif->in_q_buf, NETIF_INQ_SIZE, LOCKER_THREAD);
    if (err < 0) {
        log_error(LOG_NETIF, "netif in_q init error, err: %d", err);
        return (netif_t *)0;
    }

    err = fixed_queue_init(&netif->out_q, netif->out_q_buf, NETIF_OUTQ_SIZE, LOCKER_THREAD);
    if (err < 0) {
        log_error(LOG_NETIF, "netif out_q init error, err: %d", err);
        fixed_queue_destroy(&netif->in_q);
        return (netif_t *)0;
    }

    err = ops->open(netif, ops_data);
    if (err < 0) {
        log_error(LOG_NETIF, "netif ops open error: %d");
        goto free_return;
    }
    netif->link_layer = netif_get_layer(netif->type);
    if (!netif->link_layer && (netif->type != NETIF_TYPE_LOOP)) {
        log_error(LOG_NETIF, "no link layer. netif name: %s", dev_name);
        goto free_return;
    }
    netif->state = NETIF_OPENED;

    if (netif->type == NETIF_TYPE_NONE) {
        log_error(LOG_NETIF, "netif type unknown");
        goto free_return;
    }
    list_insert_last(&netif_list, &netif->node);
    display_netif_list();
    return netif;
    free_return:
    if (netif->state == NETIF_OPENED) {
        netif->ops->close(netif);
    }
    fixed_queue_destroy(&netif->in_q);
    fixed_queue_destroy(&netif->out_q);
    memory_pool_free(&netif_pool, netif);
    return (netif_t*)0;
}


net_err_t netif_set_addr(netif_t* netif, ipaddr_t* ip, ipaddr_t* netmask, ipaddr_t* gateway) {
    ipaddr_copy(&netif->ipaddr, ip ? ip : ipaddr_get_any());
    ipaddr_copy(&netif->netmask, netmask ? netmask : ipaddr_get_any());
    ipaddr_copy(&netif->gateway, gateway ? gateway : ipaddr_get_any());
    return NET_OK;
}


net_err_t netif_set_hwaddr(netif_t* netif, const uint8_t* hwaddr, int len) {
    plat_memcpy(netif->hwaddr.addr, hwaddr, len);
    netif->hwaddr.len = len;
    return NET_OK;
}


net_err_t netif_set_active(netif_t* netif) {
    if (netif->state != NETIF_OPENED) {
        log_error(LOG_NETIF, "netif is not opened");
        return NET_ERR_STATE;
    }
    if (netif->link_layer) {
        net_err_t err = netif->link_layer->open(netif);
        if (err < 0) {
            log_info(LOG_NETIF, "active error.");
            return err;
        }
    }
    // default netif can not be loopback
    if (!netif_default && (netif->type != NETIF_TYPE_LOOP)) {
        netif_set_default(netif);
    }

    netif->state = NETIF_ACTIVE;
    display_netif_list();
    return NET_OK;
}


net_err_t netif_set_deactive(netif_t* netif) {
    if (netif->state != NETIF_ACTIVE) {
        log_error(LOG_NETIF, "netif is not actived");
        return NET_ERR_STATE;
    }
    if (netif->link_layer) {
        netif->link_layer->close(netif);
    }
    // free all packets in in_q and out_q before deactived
    packet_t* packet;
    while ((packet = fixed_queue_recv(&netif->in_q, -1))) {
        packet_free(packet);
    }
    while ((packet = fixed_queue_recv(&netif->out_q, -1))) {
        packet_free(packet);
    }

    if (netif_default == netif) {
        netif_default = (netif_t*)0;
    }

    netif->state = NETIF_OPENED;
    display_netif_list();
    return NET_OK;
}


net_err_t netif_close(netif_t* netif) {
    // must be deactivated before close
    if (netif->state == NETIF_ACTIVE) {
        log_error(LOG_NETIF, "netif(%s) is active, close failed.", netif->name);
        return NET_ERR_STATE;
    }

    // close the physical device
    netif->ops->close(netif);
    netif->state = NETIF_CLOSED;

    // free resources in memory
    list_remove(&netif_list, &netif->node);
    memory_pool_free(&netif_pool, netif);
    display_netif_list();
    return NET_OK;
}


void netif_set_default(netif_t* netif) {
    netif_default = netif;
}

/**
 * put a packet into the input queue of the network interface
 * and notify the message handler thread
 */
net_err_t netif_put_in(netif_t* netif, packet_t * packet, int tmo) {
    net_err_t err = fixed_queue_send(&netif->in_q, packet, tmo);
    if (err < 0) {
        log_warning(LOG_NETIF, "netif %s in_q full", netif->name);
        return NET_ERR_FULL;
    }

    // notify the message handler thread
    handler_netif_in(netif);
    return NET_OK;
}

/**
 * put a packet into the output queue of the network interface
 */
net_err_t netif_put_out(netif_t* netif, packet_t * buf, int tmo) {
    net_err_t err = fixed_queue_send(&netif->out_q, buf, tmo);
    if (err < 0) {
        log_warning(LOG_NETIF, "netif %s out_q full", netif->name);
        return err;
    }
    return NET_OK;
}

/**
 * get a packet from the input queue of the network interface
 */
packet_t* netif_get_in(netif_t* netif, int tmo) {
    packet_t* packet = fixed_queue_recv(&netif->in_q, tmo);
    // reset the position of the packet
    if (packet) {
        packet_reset_pos(packet);
        return packet;
    }
    log_info(LOG_NETIF, "netif %s in_q empty", netif->name);
    return (packet_t*)0;
}

/**
 * get a packet from the output queue of the network interface
 */
packet_t* netif_get_out(netif_t* netif, int tmo) {
    packet_t* packet = fixed_queue_recv(&netif->out_q, tmo);
    if (packet) {
        packet_reset_pos(packet);
        return packet;
    }

    log_warning(LOG_NETIF, "netif %s out_q empty", netif->name);
    return (packet_t*)0;
}


net_err_t netif_out(netif_t* netif, ipaddr_t * ipaddr, packet_t* packet) {
    net_err_t err = netif_put_out(netif, packet, -1);
    if (err < 0) {
        log_warning(LOG_NETIF, "send to netif queue failed. err: %d", err);
        return err;
    }

    return netif->ops->transmit(netif);
}