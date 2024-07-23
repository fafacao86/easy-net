#ifndef EASY_NET_EASY_NET_CONFIG_H
#define EASY_NET_EASY_NET_CONFIG_H


/**
 * Configure the log level for each module.
 * */
#define LOG_MEMORY_POOL		    LEVEL_ERROR			// log level for memory pool
#define LOG_QUEUE               LEVEL_ERROR			// log level for queue
#define LOG_HANDLER             LEVEL_ERROR			// log level for handler thread
#define LOG_PACKET_BUFFER       LEVEL_ERROR			// log level for packet buffer
#define LOG_NETIF               LEVEL_ERROR			// log level for network interface
#define LOG_ETHER               LEVEL_ERROR			// log level for ethernet
#define LOG_UTILS               LEVEL_ERROR			// log level for utils
#define LOG_ARP                 LEVEL_ERROR			// log level for arp
#define LOG_IP                  LEVEL_WARNING			// log level for ip
#define LOG_TIMER               LEVEL_ERROR			// log level for timer
#define LOG_ICMP                LEVEL_INFO			// log level for icmp
#define LOG_SOCKET              LEVEL_INFO			// log level for socket
#define LOG_RAW                 LEVEL_INFO			// log level for raw socket
#define LOG_UDP                 LEVEL_INFO			// log level for udp


/**
 * Properties of the network stack.
 * */
#define NET_ENDIAN_LITTLE           1

#define HANDLER_BUFFER_SIZE         10			// size of the message buffer for the handler thread
#define HANDLER_LOCK_TYPE           LOCKER_THREAD  // type of locker for the handler thread
#define PACKET_PAGE_SIZE           128        // size of each page in a packet
#define PACKET_PAGE_CNT            512         // size of the page memory pool
#define PACKET_BUFFER_SIZE         256       // size of the packer buffer memory pool

#define TIMER_SCAN_PERIOD           500         // period of timer scan

/**
 * Link layer properties.
 * */
#define NETIF_HWADDR_SIZE           10                  // hardware address size, for mac the size is 6 bytes
#define NETIF_NAME_SIZE             16                  // network interface name size
#define NETIF_INQ_SIZE             50                  // network interface input queue size
#define NETIF_OUTQ_SIZE            50                  // network interface output queue size
#define NETIF_DEV_CNT              5                   // maximum number of network interfaces
#define ETHER_MTU                  1500                // maximum transmission unit
#define ETH_HWA_SIZE               6                   // hardware address size for ethernet
#define ETHER_MIN_PAYLOAD          46                  // minimum payload size for ethernet


/**
 * Network layer properties.
 */
#define ARP_CACHE_SIZE             50                 // size of the arp cache
#define ARP_MAX_PKT_WAIT            10                 // maximum number of packets to wait for arp reply
#define ARP_ENTRY_STABLE_TMO			1200 // (20*60)		    // timout for stable arp entry，usually 20 minutes
#define ARP_ENTRY_PENDING_TMO			3               // pending arp entry timeout， RFC1122 suggests 1 second
#define ARP_ENTRY_RETRY_CNT				3               // pending arp entry retry count
#define ARP_TIMER_TMO               1               // ARP scanner timer timeout value, in seconds
#define NET_IP_DEF_TTL                 64                 // default time-to-live value for IP packets
#define IP_FRAGS_MAX_NR               16                 // maximum number of big ip packet can be fragmented
#define IP_FRAG_MAX_BUF_NR            16                 // maximum number of fragments in a big ip packet
#define IP_FRAG_SCAN_PERIOD           1               // period of ip fragment scanner, in seconds
#define IP_FRAG_TMO                 5               // ip fragment timeout, in seconds
#define IP_RTABLE_SIZE               32                // size of the routing table


/**
* Socket properties.
*/
#define RAW_MAX_NR               100                // maximum number of raw sockets
#define RAW_MAX_RECV             50                 // raw socket receive buffer size
#define NET_PORT_DYN_START       1024               // start of dynamic port range
#define NET_PORT_DYN_END         65535              // end of dynamic port range


/**
 * UDP properties.
 * */
#define UDP_MAX_NR               50                // maximum number of udp sockets


#endif
