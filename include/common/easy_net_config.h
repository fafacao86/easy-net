#ifndef EASY_NET_EASY_NET_CONFIG_H
#define EASY_NET_EASY_NET_CONFIG_H


/**
 * Configure the log level for each module.
 * */
#define LOG_MEMORY_POOL		    LEVEL_INFO			// log level for memory pool
#define LOG_QUEUE               LEVEL_INFO			// log level for queue
#define LOG_HANDLER             LEVEL_INFO			// log level for handler thread
#define LOG_PACKET_BUFFER       LEVEL_INFO			// log level for packet buffer

/**
 * Properties of the network stack.
 * */
#define HANDLER_BUFFER_SIZE         10			// size of the message buffer for the handler thread
#define HANDLER_LOCK_TYPE           LOCKER_THREAD  // type of locker for the handler thread
#define PACKET_PAGE_SIZE           128        // size of each page in a packet
#define PACKET_PAGE_CNT            100         // size of the packer memory pool
#define PACKET_BUFFER_SIZE         100       // size of the packer buffer memory pool

#endif
