/* $Id: eth.c,v 1.1 2002/12/21 09:49:00 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/net.h>
#include <os/syscall.h>

#include <stdio.h>

static unsigned char bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static void EthHandleBindingCompletion(request_t *req)
{
    handle_t event;
    event = (handle_t) req->param;
    //wprintf(L"EthHandleBindingCompletion: signalling event\n");
    EvtSignal(NULL, event);
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
    handle_t event;
    net_hwaddr_t src, dst;

    bind = ThrGetCurrent()->info->param;
    //wprintf(L"EthHandleBinding: %04x\n", bind->type);

    pages = MemCreatePageArray(packet.raw, sizeof(packet.raw));
    event = EvtAlloc(NULL);
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

        //wprintf(L"EthHandleBinding: waiting for packet\n");
        if (!IoRequest(&cb, bind->dev, &req_eth.header))
        {
            wprintf(L"EthHandleBinding: IoRequest error\n");
            break;
        }

        SysThrWaitHandle(event);
        KeYield();

        memcpy(src.u.ethernet, packet.header.src, sizeof(src.u.ethernet));
        memcpy(dst.u.ethernet, packet.header.dst, sizeof(dst.u.ethernet));
        //wprintf(L"EthHandleBinding: got packet\n");
        if (bind->proto->vtbl->receive_packet != NULL)
            bind->proto->vtbl->receive_packet(bind->proto, 
                bind, &src, &dst, 
                packet.raw + sizeof(packet.header),
                req_eth.params.eth_receive.length - sizeof(packet.header));
    }

    HndClose(NULL, event, 'evnt');
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

    bind->thr = ThrCreateThread(NULL, true, EthHandleBinding, true, bind, 12);
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
