/* $Id: arp.c,v 1.1 2002/08/17 17:08:32 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/net.h>
#include <stdio.h>

typedef struct arp_adaptor_t arp_adaptor_t;
struct arp_adaptor_t
{
    uint32_t ip;
    uint8_t station_address[6];
};

static struct
{
    uint32_t ip;
    uint8_t mac[6];
} arp_table[16];

static spinlock_t sem_arp;
static unsigned arp_count;

bool ArpLookupIpAddress(uint32_t ip, uint8_t *eth)
{
    unsigned i;

    SpinAcquire(&sem_arp);
    for (i = 0; i < arp_count; i++)
        if (arp_table[i].ip == ip)
        {
            memcpy(eth, arp_table[i].mac, 6);
            SpinRelease(&sem_arp);
            return true;
        }

    SpinRelease(&sem_arp);
    return false;
}

void ArpAddIpAddress(uint32_t ip, const uint8_t *eth)
{
    unsigned i;

    SpinAcquire(&sem_arp);

    for (i = 0; i < arp_count; i++)
        if (arp_table[i].ip == ip ||
            memcmp(arp_table[i].mac, eth, 6) == 0)
            break;

    if (i == arp_count)
    {
        if (arp_count >= 16)
            i = 0;
        else
            i = arp_count++;

        wprintf(L"ArpAddIpAddress: %02X-%02X-%02X-%02X-%02X-%02X => %u.%u.%u.%u\n",
            eth[0], eth[1], eth[2], eth[3], eth[4], eth[5], 
            IP_A(ip), IP_B(ip), IP_C(ip), IP_D(ip));
    }

    arp_table[i].ip = ip;
    memcpy(arp_table[i].mac, eth, 6);
    SpinRelease(&sem_arp);
}

static void ArpBind(net_protocol_t *proto, net_binding_t *bind, void **cookie)
{
    arp_adaptor_t *adaptor;
    request_eth_t req_eth;

    adaptor = malloc(sizeof(arp_adaptor_t));
    assert(adaptor != NULL);
    adaptor->ip = IP_ADDRESS(192, 168, 0, 200);

    req_eth.header.code = ETH_ADAPTOR_INFO;
    if (IoRequestSync(bind->dev, &req_eth.header))
        memcpy(adaptor->station_address, 
            req_eth.params.eth_adaptor_info.station_address, 
            6);

    wprintf(L"ArpBind: binding to adaptor %02X-%02X-%02X-%02X-%02X-%02X on ip %u.%u.%u.%u\n",
        adaptor->station_address[0], adaptor->station_address[1], adaptor->station_address[2], 
        adaptor->station_address[3], adaptor->station_address[4], adaptor->station_address[5], 
        IP_A(adaptor->ip), IP_B(adaptor->ip), IP_C(adaptor->ip), IP_D(adaptor->ip));

    *cookie = adaptor;
}

static void ArpReceivePacket(net_protocol_t *proto, 
                             net_binding_t *bind,
                             const char *from, 
                             const char *to,
                             void *data, 
                             size_t length)
{
    const net_arp *arp;
    net_arp reply;
    request_eth_t req_eth;
    page_array_t *pages;
    arp_adaptor_t *adaptor;

    arp = data;
    adaptor = bind->proto_cookie;
    wprintf(L"ArpHandlePacket: length = %u ", length);

    switch (arp->arp_op)
    {
    case htons(ARP_OP_REQUEST):
        wprintf(L"ARP_OP_REQUEST: ip = %x, ip_this = %x\n", 
            arp->arp_ip_target, adaptor->ip);

        if (arp->arp_ip_target == adaptor->ip)
        {
            ArpAddIpAddress(arp->arp_ip_target, from);

            reply = *arp;
            reply.arp_op = htons(ARP_OP_REPLY);

            memcpy(&reply.arp_enet_sender, adaptor->station_address, 6);
            reply.arp_ip_sender = adaptor->ip;

            memcpy(&reply.arp_enet_target, &arp->arp_enet_sender, 6);
            reply.arp_ip_target = arp->arp_ip_sender;

            pages = MemCreatePageArray(&reply, sizeof(reply));
            req_eth.header.code = ETH_SEND;
            memcpy(req_eth.params.eth_send.to, from, 6);
            req_eth.params.eth_send.type = htons(ETH_FRAME_ARP);
            req_eth.params.eth_send.pages = pages;
            req_eth.params.eth_send.length = sizeof(reply);

            IoRequestSync(bind->dev, &req_eth.header);

            MemDeletePageArray(pages);
        }
        break;

    case htons(ARP_OP_REPLY):
        wprintf(L"ARP_OP_REPLY: ip = %x, ip_this = %x\n", 
            arp->arp_ip_target, adaptor->ip);
        ArpAddIpAddress(arp->arp_ip_target, from);
        break;

    default:
        wprintf(L"op = %04x\n", ntohs(arp->arp_op));
        break;
    }
}

static const net_protocol_vtbl_t arp_vtbl = 
{
    ArpBind,    /* bind */
    NULL,       /* unbind */
    ArpReceivePacket,
};

net_protocol_t proto_arp = 
{
    &arp_vtbl,
};
