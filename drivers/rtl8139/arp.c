/* $Id: arp.c,v 1.1 2002/12/21 09:48:59 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/net.h>

#include <stdio.h>
#include <wchar.h>

typedef struct arp_adaptor_t arp_adaptor_t;
struct arp_adaptor_t
{
    uint32_t ip;
    net_hwaddr_t *hwaddr;
};

static struct
{
    uint32_t ip;
    net_hwaddr_t *addr;   
} arp_table[16];

static spinlock_t sem_arp;
static unsigned arp_count;

int NetCompareHwAddr(const net_hwaddr_t *addr1, const net_hwaddr_t *addr2)
{
    if (addr1->type != addr2->type)
        return addr1->type - addr2->type;
    else
        return memcmp(addr1->u.raw, addr2->u.raw, 
            min(addr1->data_size, addr2->data_size));
}

net_hwaddr_t *NetCopyHwAddr(const net_hwaddr_t *addr)
{
    net_hwaddr_t *ret;

    ret = malloc(sizeof(*ret) - sizeof(ret->u) + addr->data_size);
    if (ret == NULL)
        return NULL;

    ret->type = addr->type;
    ret->data_size = addr->data_size;
    memcpy(ret->u.raw, addr->u.raw, ret->data_size);
    return ret;
}

const wchar_t *NetFormatHwAddr(const net_hwaddr_t *addr)
{
    static wchar_t ret[30];
    wchar_t *ch;
    unsigned i;

    i = 0;
    ch = ret;
    while (i < addr->data_size && ch < ret + _countof(ret) - 1)
        ch += swprintf(ch, L"%02X-", addr->u.raw[i++]);

    ch[-1] = '\0';
    return ret;
}

const net_hwaddr_t *ArpLookupIpAddress(uint32_t ip)
{
    unsigned i;
    const net_hwaddr_t *ret;

    SpinAcquire(&sem_arp);
    for (i = 0; i < arp_count; i++)
        if (arp_table[i].ip == ip)
        {
            ret = arp_table[i].addr;
            SpinRelease(&sem_arp);
            return ret;
        }

    SpinRelease(&sem_arp);
    return NULL;
}

void ArpAddIpAddress(uint32_t ip, const net_hwaddr_t *addr)
{
    unsigned i;

    SpinAcquire(&sem_arp);

    for (i = 0; i < arp_count; i++)
        if (arp_table[i].ip == ip ||
            (arp_table[i].addr != NULL && 
                NetCompareHwAddr(addr, arp_table[i].addr) == 0))
            break;

    if (i == arp_count)
    {
        if (arp_count >= 16)
            i = 0;
        else
            i = arp_count++;

        wprintf(L"ArpAddIpAddress: %s => %u.%u.%u.%u\n",
            NetFormatHwAddr(addr), 
            IP_A(ip), IP_B(ip), IP_C(ip), IP_D(ip));
    }

    if (arp_table[i].addr != NULL)
        free(arp_table[i].addr);

    arp_table[i].ip = ip;
    arp_table[i].addr = NetCopyHwAddr(addr);
    SpinRelease(&sem_arp);
}

static void ArpBind(net_protocol_t *proto, net_binding_t *bind, void **cookie)
{
    arp_adaptor_t *adaptor;
    request_net_t req_net;
    union
    {
        net_hwaddr_t addr;
        uint8_t buf[14];
    } u;

    adaptor = malloc(sizeof(arp_adaptor_t));
    assert(adaptor != NULL);
    adaptor->ip = IP_ADDRESS(192, 168, 0, 200);

    u.addr.type = 0;
    u.addr.data_size = 0;
    req_net.header.code = NET_GET_HW_INFO;
    req_net.params.net_hw_info.addr_data_size = sizeof(u.buf);
    req_net.params.net_hw_info.addr = &u.addr;
    IoRequestSync(bind->dev, &req_net.header);
    adaptor->hwaddr = NetCopyHwAddr(&u.addr);

    wprintf(L"ArpBind: binding to adaptor %s on ip %u.%u.%u.%u\n",
        NetFormatHwAddr(adaptor->hwaddr),
        IP_A(adaptor->ip), IP_B(adaptor->ip), IP_C(adaptor->ip), IP_D(adaptor->ip));

    *cookie = adaptor;
}

static void ArpUnbind(net_protocol_t *proto, net_binding_t *bind, void *cookie)
{
    arp_adaptor_t *adaptor;
    adaptor = cookie;
    free(adaptor->hwaddr);
}

static void ArpReceivePacket(net_protocol_t *proto, 
                             net_binding_t *bind,
                             const net_hwaddr_t *from, 
                             const net_hwaddr_t *to,
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

            memcpy(&reply.arp_enet_sender, adaptor->hwaddr->u.ethernet, 6);
            reply.arp_ip_sender = adaptor->ip;

            memcpy(&reply.arp_enet_target, &arp->arp_enet_sender, 6);
            reply.arp_ip_target = arp->arp_ip_sender;

            pages = MemCreatePageArray(&reply, sizeof(reply));
            req_eth.header.code = ETH_SEND;
            memcpy(req_eth.params.eth_send.to, from->u.ethernet, 6);
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
    ArpBind,
    ArpUnbind,
    ArpReceivePacket,
};

net_protocol_t proto_arp = 
{
    &arp_vtbl,
};
