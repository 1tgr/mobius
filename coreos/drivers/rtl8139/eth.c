/*
 * $History: eth.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:30
 * Updated in $/coreos/drivers/rtl8139
 * Updated to keep in line with kernel
 */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/net.h>

#include <stdio.h>
#include <string.h>

static unsigned char bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static void EthHandleBindingCompletion(request_t *req)
{
    EvtSignal(req->param, true);
}


static void EthHandleBinding(void)
{
    net_binding_t *bind;
    request_eth_t req_eth;

    union
    {
        uint8_t raw[ETH_FRAME_LEN];
        net_ether header;
    } packet;

    page_array_t *pages;
    io_callback_t cb;
    event_t *event;
    net_hwaddr_t src, dst;

    bind = ThrGetCurrent()->info->param;

    pages = MemCreatePageArray(packet.raw, sizeof(packet.raw));
    event = EvtCreate(false);
    src.type = dst.type = NET_HW_ETHERNET;
    src.data_size = dst.data_size = 6;

    while (true)
    {
        req_eth.header.code = ETH_RECEIVE;
        req_eth.header.param = (void*) event;
        memcpy(req_eth.params.eth_receive.from, bcast, sizeof(bcast));
        req_eth.params.eth_receive.type = (uint16_t) (addr_t) bind->hw_cookie;
        req_eth.params.eth_receive.pages = pages;
        req_eth.params.eth_receive.length = sizeof(packet.raw);
        cb.type = IO_CALLBACK_FUNCTION;
        cb.u.function = EthHandleBindingCompletion;

        if (!IoRequest(&cb, bind->dev, &req_eth.header))
        {
            wprintf(L"EthHandleBinding: IoRequest error\n");
            break;
        }

        ThrWaitHandle(ThrGetCurrent(), &event->hdr);
        KeYield();

        memcpy(src.u.ethernet, packet.header.src, sizeof(src.u.ethernet));
        memcpy(dst.u.ethernet, packet.header.dst, sizeof(dst.u.ethernet));
        if (bind->proto->vtbl->receive_packet != NULL)
            bind->proto->vtbl->receive_packet(bind->proto, 
                bind, &src, &dst, 
                packet.raw + sizeof(packet.header),
                req_eth.params.eth_receive.length - sizeof(packet.header));
    }

    HndClose(&event->hdr);
    ThrExitThread(0);
    KeYield();
}


net_binding_t *EthBindProtocol(net_protocol_t *proto, device_t *dev, uint16_t type)
{
    net_binding_t *bind;

    bind = malloc(sizeof(net_binding_t));
    if (bind == NULL)
        return NULL;

    bind->dev = dev;
    bind->hw_cookie = (void*) (addr_t) type;
    bind->proto = proto;
    bind->proto_cookie = NULL;

    bind->thr = ThrCreateThread(NULL, true, EthHandleBinding, true, bind, 12, L"EthHandleBinding");
    if (bind->thr == NULL)
    {
        free(bind);
        return NULL;
    }

    if (proto->vtbl->bind != NULL)
        proto->vtbl->bind(proto, bind, &bind->proto_cookie);

    return bind;
}


void EthUnbindProtocol(net_binding_t *bind)
{
    if (bind->proto->vtbl->unbind != NULL)
        bind->proto->vtbl->unbind(bind->proto, bind, bind->proto_cookie);
    /* xxx - stop protocol thread */
}
