/* $Id: net.h,v 1.1 2002/08/14 16:30:53 pavlovskii Exp $
**
** Copyright 1998 Brian J. Swetland
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions, and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions, and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __KERNEL_NET_H
#define __KERNEL_NET_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/memory.h>
#include <kernel/driver.h>

typedef union params_eth_t params_eth_t;
union params_eth_t
{
    struct
    {
        uint8_t addr[6];
        uint16_t type;
        page_array_t *pages;
        size_t length;
    } buffered;

    struct
    {
        uint8_t from[6];            /* in/out */
        uint16_t type;              /* in */
        page_array_t *pages;        /* buffer: out */
        size_t length;              /* in/out */
    } eth_receive;

    struct
    {
        uint8_t to[6];              /* in */
        uint16_t type;              /* in */
        page_array_t *pages;        /* buffer: in */
        size_t length;              /* in */
    } eth_send;

    struct
    {
        uint8_t station_address[6]; /* out */
        size_t mtu;                 /* out */
    } eth_adaptor_info;
};

typedef struct request_eth_t request_eth_t;
struct request_eth_t
{
    request_t header;
    params_eth_t params;
};

#pragma pack(push, 2)
typedef struct 
{
    uint8_t  dst[6];
    uint8_t  src[6];    
    uint16_t type;    
} net_ether;

typedef struct 
{
    uint8_t  ip_hdr_len:4;
    uint8_t  ip_version:4;
    uint8_t  ip_tos;
    uint16_t ip_len;
    
    uint16_t ip_id;
    uint16_t ip_off;
    
    uint8_t  ip_ttl;
    uint8_t  ip_proto;    
    uint16_t ip_chk;
    
    uint32_t ip_src;
    uint32_t ip_dst;        
} net_ip;

typedef struct
{
    uint16_t udp_src;
    uint16_t udp_dst;

    uint16_t udp_len;
    uint16_t udp_chk;    
} net_udp;

typedef struct
{
    uint8_t  icmp_type;
    uint8_t  icmp_code;
    uint16_t icmp_chk;
} net_icmp;

typedef struct
{
    net_icmp ping_icmp;    
    uint16_t ping_id;
    uint16_t ping_seq;    
} net_icmp_ping;

typedef struct
{
    uint16_t arp_hard_type;
    uint16_t arp_prot_type;    
    uint8_t  arp_hard_size;
    uint8_t  arp_prot_size;
    uint16_t arp_op;    
    uint8_t  arp_enet_sender[6];
    uint32_t arp_ip_sender;
    uint8_t  arp_enet_target[6];
    uint32_t arp_ip_target;    
} net_arp;
#pragma pack(pop)

#define ETH_FRAME_IP        0x0800
#define ETH_FRAME_ARP       0x0806

#define ETH_ALEN            6       /* Size of Ethernet address */
#define ETH_HLEN            14      /* Size of ethernet header */
#define ETH_ZLEN            60      /* Minimum packet */
#define ETH_FRAME_LEN       1514    /* Maximum packet */
#define ETH_MAX_MTU        (ETH_FRAME_LEN - ETH_HLEN)

#define IP_PROTO_ICMP       1
#define IP_PROTO_IGMP       2
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

#define IP_TOS_MIN_DELAY    0x10
#define IP_TOS_MAX_THRU     0x08
#define IP_TOS_MAX_RELY     0x04
#define IP_TOS_MIN_COST     0x02

#define IP_FLAG_CE          0x8000
#define IP_FLAG_DF          0x4000
#define IP_FLAG_MF          0x2000
#define IP_FLAG_MASK        0x1FFF

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2
#define RARP_OP_REQUEST 3
#define RARP_OP_REPLY   4

/* yeah, ugly as shit */
#define ntohs(n) ( (((n) & 0xFF00) >> 8) | (((n) & 0x00FF) << 8) )
#define htons(n) ( (((n) & 0xFF00) >> 8) | (((n) & 0x00FF) << 8) )
#define ntohl(n) ( (((n) & 0xFF000000) >> 24) | (((n) & 0x00FF0000) >> 8) | (((n) & 0x0000FF00) << 8) | (((n) & 0x000000FF) << 24) )
#define htonl(n) ( (((n) & 0xFF000000) >> 24) | (((n) & 0x00FF0000) >> 8) | (((n) & 0x0000FF00) << 8) | (((n) & 0x000000FF) << 24) )

#define IP_ADDRESS(a, b, c, d)  ((a) | (b) << 8 | (c) << 16 | (d) << 24)
#define IP_A(ip)                ((uint8_t) ((ip) >>  0))
#define IP_B(ip)                ((uint8_t) ((ip) >>  8))
#define IP_C(ip)                ((uint8_t) ((ip) >> 16))
#define IP_D(ip)                ((uint8_t) ((ip) >> 24))

typedef struct net_binding_t net_binding_t;
typedef struct net_protocol_t net_protocol_t;

typedef struct net_protocol_vtbl_t net_protocol_vtbl_t;
struct net_protocol_vtbl_t
{
    void (*bind)(net_protocol_t *proto, net_binding_t *bind, void **cookie);
    void (*unbind)(net_protocol_t *proto, net_binding_t *bind, void *cookie);
    void (*receive_packet)(net_protocol_t *proto, net_binding_t *bind, 
        const char *from, const char *to, 
        void *data, size_t length);
};

struct net_protocol_t
{
    const net_protocol_vtbl_t *vtbl;
};

struct net_binding_t
{
    device_t *dev;
    struct thread_t *thr;
    uint16_t type;
    net_protocol_t *proto;
    void *hw_cookie;
    void *proto_cookie;
};

net_binding_t *EthBindProtocol(net_protocol_t *proto, device_t *dev, uint16_t type);
void    EthUnbindProtocol(net_binding_t *bind);

bool    ArpLookupIpAddress(uint32_t ip, uint8_t *eth);
void    ArpAddIpAddress(uint32_t ip, const uint8_t *eth);

#ifdef __cplusplus
}
#endif

#endif
