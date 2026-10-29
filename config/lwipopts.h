/*
 * lwipopts.h - lwIP for a PROFINET device: IPv4 with ARP, ICMP (ping) and UDP (PNIO-CM, port 34964)
 * behind the BSD socket API, on FreeRTOS. No TCP, DHCP or IPv6: the address comes by DCP.
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* OS: FreeRTOS (lwIP's contrib sys_arch), one tcpip thread, the core lock for direct calls */
#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_TCPIP_CORE_LOCKING         1
#define LWIP_TCPIP_CORE_LOCKING_INPUT   0
#define LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS 0   // stack sizes below in bytes
#define TCPIP_THREAD_NAME               "tcpip"
#define TCPIP_THREAD_STACKSIZE          4096
#define TCPIP_THREAD_PRIO               20               // below the PROFINET tasks (config.h)
#define TCPIP_MBOX_SIZE                 16
#define DEFAULT_UDP_RECVMBOX_SIZE       8
#define DEFAULT_RAW_RECVMBOX_SIZE       4
#define DEFAULT_TCP_RECVMBOX_SIZE       4
#define DEFAULT_ACCEPTMBOX_SIZE         4
#define LWIP_COMPAT_MUTEX               0

/* the C library: newlib */
#define LWIP_ERRNO_STDINCLUDE           1
#define LWIP_PROVIDE_ERRNO              0
#define LWIP_TIMEVAL_PRIVATE            0

/* Memory */
#define MEM_LIBC_MALLOC                 0
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                6
#define MEMP_NUM_NETBUF                 8
#define MEMP_NUM_NETCONN                6
#define MEMP_NUM_TCPIP_MSG_INPKT        16
#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1536   // a whole Ethernet frame in one pbuf

/* Protocols */
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define ETHARP_SUPPORT_VLAN             0
#define LWIP_ICMP                       1
#define LWIP_RAW                        0
#define LWIP_UDP                        1
#define LWIP_TCP                        0
#define LWIP_DHCP                       0
#define LWIP_AUTOIP                     0
#define LWIP_ACD                        0
#define LWIP_IGMP                       0
#define LWIP_DNS                        0
#define IP_FORWARD                      0
#define IP_REASSEMBLY                   1
#define IP_FRAG                         1
#define IP_REASS_MAX_PBUFS              8

/* APIs: sockets for pnal_udp_rp2.c, with the lwip_ names only (newlib has its own close() ...) */
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1
#define LWIP_COMPAT_SOCKETS             0
#define LWIP_POSIX_SOCKETS_IO_NAMES     0
#define LWIP_SO_RCVTIMEO                0
#define LWIP_SO_RCVBUF                  0
#define SO_REUSE                        1   // pnal_udp_open() sets SO_REUSEADDR
#define LWIP_NETIF_API                  0

/* The interface */
#define LWIP_SINGLE_NETIF               1
#define LWIP_NETIF_STATUS_CALLBACK      1   // eth.c logs each new address
#define LWIP_NETIF_LINK_CALLBACK        0
#define LWIP_NETIF_HOSTNAME             0
#define LWIP_NETIF_TX_SINGLE_PBUF       0
#define ETH_PAD_SIZE                    0

/* Off */
#define LWIP_STATS                      0
#define LWIP_DEBUG                      0
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_ICMP             1

#endif /* LWIPOPTS_H */
