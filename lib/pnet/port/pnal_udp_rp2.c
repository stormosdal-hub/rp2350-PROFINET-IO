/*
 * pnal_udp_rp2.c - UDP for p-net (PNIO-CM on port 34964: connect, read, write, release) on lwIP
 * sockets. Receiving never blocks: p-net polls from its periodic tick. The lwip_ names, since
 * lwipopts.h leaves out the BSD names (they would clash with newlib's).
 */
#include <string.h>
#include "lwip/sockets.h"
#include "pnal.h"

int pnal_udp_open(pnal_ipaddr_t addr, pnal_ipport_t port)
{
    const int enable = 1;
    int id = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (id < 0) return -1;

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(addr);
    local.sin_port = htons(port);
    if (lwip_setsockopt(id, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0 ||
        lwip_bind(id, (struct sockaddr *)&local, sizeof(local)) != 0) {
        lwip_close(id);
        return -1;
    }
    return id;
}

int pnal_udp_sendto(uint32_t id, pnal_ipaddr_t dst_addr, pnal_ipport_t dst_port, const uint8_t *data, int size)
{
    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(dst_addr);
    remote.sin_port = htons(dst_port);
    return lwip_sendto((int)id, data, size, 0, (struct sockaddr *)&remote, sizeof(remote));
}

int pnal_udp_recvfrom(uint32_t id, pnal_ipaddr_t *src_addr, pnal_ipport_t *src_port, uint8_t *data, int size)
{
    struct sockaddr_in remote;
    socklen_t addr_len = sizeof(remote);
    memset(&remote, 0, sizeof(remote));
    int len = lwip_recvfrom((int)id, data, size, MSG_DONTWAIT, (struct sockaddr *)&remote, &addr_len);
    if (len > 0) {
        *src_addr = ntohl(remote.sin_addr.s_addr);
        *src_port = ntohs(remote.sin_port);
    }
    return len;
}

void pnal_udp_close(uint32_t id) { lwip_close((int)id); }
