#ifndef EASY_NET_EASY_NET_CONFIG_H
#define EASY_NET_EASY_NET_CONFIG_H


/**
 * Configure the log level for each module.
 * */
#define LOG_MEMORY_POOL		    LEVEL_INFO			// log level for memory pool
#define LOG_QUEUE               LEVEL_INFO			// log level for queue
#define LOG_HANDLER             LEVEL_INFO			// log level for handler thread
#define LOG_PACKET_BUFFER       LEVEL_INFO			// log level for packet buffer
#define LOG_NETIF               LEVEL_INFO			// log level for network interface

/**
 * Properties of the network stack.
 * */
#define HANDLER_BUFFER_SIZE         10			// size of the message buffer for the handler thread
#define HANDLER_LOCK_TYPE           LOCKER_THREAD  // type of locker for the handler thread
#define PACKET_PAGE_SIZE           128        // size of each page in a packet
#define PACKET_PAGE_CNT            100         // size of the packer memory pool
#define PACKET_BUFFER_SIZE         100       // size of the packer buffer memory pool

/**
 * Access layer properties.
 * */
#define NETIF_HWADDR_SIZE           10                  // hardware address size, for mac the size is 6 bytes
#define NETIF_NAME_SIZE             16                  // network interface name size
#define NETIF_INQ_SIZE             50                  // network interface input queue size
#define NETIF_OUTQ_SIZE            50                  // network interface output queue size
#define NETIF_DEV_CNT              5                   // maximum number of network interfaces
#define NETIF_MTU                  1500                // maximum transmission unit
#endif
