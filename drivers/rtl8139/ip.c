/* $Id: ip.c,v 1.1 2002/12/21 09:49:00 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/net.h>

#include <kernel/debug.h>

#include <os/syscall.h>
#include <stdio.h>
#include <errno.h>

static uint16_t IpChecksum(const void *buf, size_t len)
{
    unsigned long sum = 0;
    const uint16_t *ip1;

    ip1 = buf;
    while (len > 1)
    {
        sum += *ip1++;
        if (sum & 0x80000000)
            sum = (sum & 0xffff) + (sum >> 16);
        len -= 2;
    }

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

static status_t IpSendPacket(net_binding_t *bind, uint32_t to, uint8_t proto, 
                             void *data, size_t length)
{
    net_ip *packet;
    const net_hwaddr_t *hwto;
    page_array_t *pages;
    request_eth_t req_eth;
    status_t ret;
    size_t length_packet;

    length_packet = sizeof(net_ip) + length;
    packet = malloc(length_packet);
    if (packet == NULL)
        return errno;

    packet->ip_hdr_len = 5;
    packet->ip_version = 4;
    packet->ip_tos = 0;
    packet->ip_len = htons(length_packet);
    packet->ip_id = 0xbeef;
    packet->ip_off = 0;
    packet->ip_ttl = 32;
    packet->ip_proto = proto;
    packet->ip_chk = 0;
    packet->ip_dst = to;
    packet->ip_src = IP_ADDRESS(192, 168, 0, 200);
    packet->ip_chk = IpChecksum(packet, sizeof(net_ip));
    memcpy(packet + 1, data, length);

    hwto = ArpLookupIpAddress(to);

#ifdef DEBUG
    wprintf(L"IpSendPacket: %u.%u.%u.%u => %02X-%02X-%02X-%02X-%02X-%02X\n",
        IP_A(packet->ip_src), IP_B(packet->ip_src), 
        IP_C(packet->ip_src), IP_D(packet->ip_src),
        to_mac[0], to_mac[1], to_mac[2], to_mac[3], to_mac[4], to_mac[5]);
#endif

    pages = MemCreatePageArray(packet, length_packet);
    req_eth.header.code = ETH_SEND;
    memcpy(req_eth.params.eth_send.to, hwto->u.ethernet, 6);
    req_eth.params.eth_send.type = htons(ETH_FRAME_IP);
    req_eth.params.eth_send.pages = pages;
    req_eth.params.eth_send.length = length_packet;

    ret = IoRequestSync(bind->dev, &req_eth.header);

    MemDeletePageArray(pages);
    free(packet);

    return ret;
}

static void IpHandleIcmp(net_protocol_t *proto, net_binding_t *bind, net_ip *header)
{
    net_icmp_ping *ping;
    size_t len;

    ping = (net_icmp_ping*) (header + 1);
    TRACE1("IpHandleIcmp: type = %u => ", ping->ping_icmp.icmp_type);
    switch (ping->ping_icmp.icmp_type)
    {
    case 8:
        len = ntohs(header->ip_len) - sizeof(net_ip);
        TRACE1("ping, %u bytes\n", len);
        ping->ping_icmp.icmp_type = 0;
        ping->ping_icmp.icmp_chk = 0;
        ping->ping_icmp.icmp_chk = IpChecksum(ping, len);
        IpSendPacket(bind, header->ip_src, IP_PROTO_ICMP, ping, len);
        break;

    default:
        TRACE0("unknown\n");
        break;
    }
}

void IpPing(net_binding_t *bind, uint32_t ip)
{
    struct
    {
        net_ip ip;
        net_icmp_ping ping;
        uint8_t data[128 - sizeof(net_ip) + sizeof(net_icmp_ping)];
    } packet;

    page_array_t *pages;
    const net_hwaddr_t *hwto;
    request_eth_t req_eth;
    unsigned i;

    pages = MemCreatePageArray(&packet, sizeof(packet));
    packet.ip.ip_hdr_len = 5;
    packet.ip.ip_version = 4;
    packet.ip.ip_tos = 0;
    packet.ip.ip_len = htons(sizeof(packet));
    packet.ip.ip_id = 0xdead;
    packet.ip.ip_off = 0;
    packet.ip.ip_ttl = 32;
    packet.ip.ip_proto = IP_PROTO_ICMP;
    packet.ip.ip_chk = 0;
    packet.ip.ip_dst = ip;
    packet.ip.ip_src = IP_ADDRESS(192, 168, 0, 200);
    packet.ip.ip_chk = IpChecksum(&packet.ip, sizeof(packet.ip));

    for (i = 0; i < _countof(packet.data); i++)
        packet.data[i] = 'a' + i % 26;

    packet.ping.ping_icmp.icmp_type = 8;
    packet.ping.ping_icmp.icmp_code = 0;
    packet.ping.ping_icmp.icmp_chk = 0;
    packet.ping.ping_id = 0xbeef;
    packet.ping.ping_seq = 0;
    packet.ping.ping_icmp.icmp_chk = 
        IpChecksum(&packet.ping, sizeof(packet.ping) + sizeof(packet.data));

    hwto = ArpLookupIpAddress(packet.ip.ip_dst);

#ifdef DEBUG
    wprintf(L"IpPing: %u.%u.%u.%u => %02X-%02X-%02X-%02X-%02X-%02X\n",
        IP_A(packet.ip.ip_src), IP_B(packet.ip.ip_src), 
        IP_C(packet.ip.ip_src), IP_D(packet.ip.ip_src),
        to[0], to[1], to[2], to[3], to[4], to[5]);
#endif

    req_eth.header.code = ETH_SEND;
    memcpy(req_eth.params.eth_send.to, hwto->u.ethernet, 6);
    req_eth.params.eth_send.type = htons(ETH_FRAME_IP);
    req_eth.params.eth_send.pages = pages;
    req_eth.params.eth_send.length = sizeof(packet);

    IoRequestSync(bind->dev, &req_eth.header);

    MemDeletePageArray(pages);
}

static void IpBind(net_protocol_t *proto, net_binding_t *bind, void **cookie)
{
}

static void IpReceivePacket(net_protocol_t *proto, 
                            net_binding_t *bind,
                            const net_hwaddr_t *from, 
                            const net_hwaddr_t *to,
                            void *data, 
                            size_t length)
{
    net_ip *header;

    header = data;
#ifdef DEBUG
    wprintf(L"IpHandlePacket: length = %u, proto = %u, src = %u.%u.%u.%u, dst = %u.%u.%u.%u\n", 
        length, header->ip_proto, 
        IP_A(header->ip_src), IP_B(header->ip_src), IP_C(header->ip_src), IP_D(header->ip_src), 
        IP_A(header->ip_dst), IP_B(header->ip_dst), IP_C(header->ip_dst), IP_D(header->ip_dst));
#endif

    ArpAddIpAddress(header->ip_src, from);
    switch (header->ip_proto)
    {
    case IP_PROTO_ICMP:
        IpHandleIcmp(proto, bind, header);
        break;
    }
}

static const net_protocol_vtbl_t ip_vtbl = 
{
    IpBind,
    NULL,   /* unbind */
    IpReceivePacket,
};

net_protocol_t proto_ip = 
{
    &ip_vtbl,
};
