/* $Id: ipc.c,v 1.1 2002/12/18 23:17:59 pavlovskii Exp $ */

#include <os/gui.h>
#include <os/rtl.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

const wchar_t *ProcGetExeName(void)
{
    wchar_t *ret;
    ret = wcsrchr(ProcGetProcessInfo()->module_first->name, '/');
    if (ret == NULL)
        return L"???";
    else
        return ret + 1;
}

ipc_packet_t *IpcBeginCall(uint32_t code)
{
    ipc_packet_t *packet;

    packet = malloc(max(sizeof(ipc_packet_t), 0));
    packet->code = code;
    packet->length = max(sizeof(ipc_packet_t), 0);
    packet->params_length = 0;
    packet->num_params = 0;

    //_wdprintf(L"IpcBeginCall: %lx\n", code);
    return packet;
}

ipc_packet_t *IpcAddParameter(ipc_packet_t *packet, const void *data, 
                              size_t size, uint32_t flags)
{
    size_t new_length;
    size_t *dest;

    new_length = packet->params_length + sizeof(size_t);
    if (data == NULL)
    {
        size = 0;
        flags |= IPC_PARAM_NULL;
    }

    if (flags & IPC_PARAM_IN)
        new_length += size;

    if (sizeof(*packet) + new_length > packet->length)
    {
        //packet->length = (sizeof(*packet) + new_length + 63) & -64;
        packet->length = sizeof(*packet) + new_length;
        packet = realloc(packet, packet->length);
        _wdprintf(L"[%s] IpcAddParameter: realloc'd to %u bytes\n", 
            ProcGetExeName(), packet->length);
    }

    /*_wdprintf(L"[%s] IpcAddParameter: adding parameter %p @ %u bytes\n", 
        ProcGetExeName(), data, size);*/
    dest = (size_t*) ((uint8_t*) packet->data + packet->params_length);
    *dest = size | flags;
    if (flags & IPC_PARAM_IN)
        memcpy(dest + 1, data, size);
    packet->params_length = new_length;
    packet->num_params++;
    return packet;
}

ipc_packet_t *IpcReceivePacket(handle_t pipe)
{
    ipc_packet_t *packet;
    size_t bytes, total;

    packet = malloc(max(64, sizeof(*packet)));
    total = 0;
    while (total < sizeof(*packet))
        if (FsReadSync(pipe, 
            (uint8_t*) packet + total, 
            sizeof(*packet) - total, 
            &bytes))
        {
            total += bytes;
            _wdprintf(L"[%u]", total);
        }
        else
        {
            free(packet);
            return NULL;
        }

    packet = realloc(packet, packet->length);
    total = 0;
    while (total < packet->length - sizeof(*packet))
        if (FsReadSync(pipe, 
            (uint8_t*) packet->data + total, 
            packet->length - sizeof(*packet) - total, 
            &bytes))
        {
            total += bytes;
            _wdprintf(L"{%u}", total);
        }
        else
        {
            free(packet);
            return NULL;
        }

    return packet;
}

uint32_t IpcDispatchCall(handle_t pipe, ipc_packet_t *packet, bool want_reply)
{
    uint32_t ret;

    _wdprintf(L"IpcDispatchCall: %p, code = %lx\n", packet, packet->code);
    FsWriteSync(pipe, packet, packet->length, NULL);

    if (!want_reply)
        return true;

    free(packet);

    packet = IpcReceivePacket(pipe);
    if (packet == NULL)
        return 0;
    else
    {
        ret = *(uint32_t*) ((size_t*) packet->data + 1);
        _wdprintf(L"[%s] IpcDispatchCall: got return value %u, code = %x\n", 
            ProcGetExeName(), ret, packet->code);
        return ret;
    }
}

bool IpcGetNextParameter(ipc_packet_t *packet, void **it, void *buf, size_t max)
{
    size_t *size;

    if (*it == NULL)
        *it = packet->data;

    size = *it;
    wprintf(L"[%s] IpcGetNextParameter: size = %u\n", ProcGetExeName(), *size);
    memcpy(buf, size + 1, min(max, *size & ~IPC_FLAGS_MASK));
    *it = ((uint8_t*) (size + 1) + (*size & ~IPC_FLAGS_MASK));
    return true;
}
