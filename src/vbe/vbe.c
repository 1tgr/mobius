/* $Id: vbe.c,v 1.3 2002/12/18 22:56:14 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>

uint8_t *vbe_code;
unsigned vbe_num_modes;
volatile bool vbe_failed;

FARPTR i386LinearToFp(void *ptr)
{
    unsigned seg, off;
    off = (addr_t) ptr & 0xf;
    seg = ((addr_t) ptr - ((addr_t) ptr & 0xf)) / 16;
    return MK_FP(seg, off);
}

#pragma pack(push, 1)

typedef struct vbe_info_t vbe_info_t;
struct vbe_info_t
{
    uint8_t signature[4];
    uint16_t version;
    FARPTR oem_name_ptr;
    uint8_t capabilities[4];
    FARPTR modes_ptr;
    uint16_t video_memory_size;
};

typedef struct vbe_mode_t vbe_mode_t;
struct vbe_mode_t
{
    uint16_t attributes;
    uint8_t win_a_attributes;
    uint8_t win_b_attributes;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t win_a_segment;
    uint16_t win_b_segment;
    FARPTR win_function_ptr;
    uint16_t bytes_per_line;

    /* VBE 1.2 part */
    uint16_t width;
    uint16_t height;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t num_planes;
    uint8_t bits_per_pixel;
    uint8_t num_banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t num_image_pages;
    uint8_t reserved;
    uint8_t red_bits;
    uint8_t red_shift;
    uint8_t green_bits;
    uint8_t green_shift;
    uint8_t blue_bits;
    uint8_t blue_shift;
    uint8_t reserved_bits;
    uint8_t reserved_shift;
    uint8_t direct_colour_info;
};

#pragma pack(pop)

void VbeV86Handler(void)
{
    context_v86_t ctx;
    uint8_t *ip;
    vbe_info_t *info;
    uint16_t *modes;
    vbe_mode_t *mode;
    unsigned i;

    ThrGetV86Context(&ctx);
    
    ip = FP_TO_LINEAR(ctx.cs, ctx.eip);
    if (ip[0] == 0xcd && ip[1] == 0x20)
    {
        uint16_t *status;

        ctx.eip += 2;
        status = (uint16_t*) (vbe_code + *(uint16_t*) (vbe_code + 0x103));
        if (*status & 0xff00)
        {
            printf("VbeV86Handler: error %x\n", *status);
            vbe_failed = true;
            ThrExitThread(0);
        }
        
        info = (vbe_info_t*) (vbe_code + *(uint16_t*) (vbe_code + 0x105));
        mode = (vbe_mode_t*) (vbe_code + *(uint16_t*) (vbe_code + 0x107));
        /*printf("status = %x, eax = %lx cs:ip = %04lx:%04lx\n", 
            *status, ctx.regs.eax, ctx.cs, ctx.eip);*/
        
        switch (*status)
        {
        case 0: /* V86 code finished */
            printf("V86 code finished\n");
            ThrExitThread(0);
            break;

        case 1: /* done VBE installation check */
            printf("VBE signature = %c%c%c%c, version = %x\n", 
                info->signature[0], info->signature[1], info->signature[2], info->signature[3],
                info->version);
            if (memcmp(info->signature, "VESA", 4) == 0)
            {
                if (info->version < 0x0102)
                {
                    printf("VESA BIOS not sufficiently recent: needs version 0102, got version %04x\n",
                        info->version);
                    ThrExitThread(0);
                }

                modes = FP_TO_LINEAR(FP_SEG(info->modes_ptr), FP_OFF(info->modes_ptr));
                printf("Mode list at %04lx:%04lx = %p\n", 
                    FP_SEG(info->modes_ptr), FP_OFF(info->modes_ptr), modes);
            
                printf("Supported modes: ");
                for (i = 0; modes[i] != 0xffff; i++)
                    printf("%x ", modes[i]);
            
                printf("\n");
                vbe_num_modes = 0;
            }
            else
            {
                printf("VBE signature apparently incorrect\n");
                ThrExitThread(0);
            }
            break;

        case 2: /* got another mode */
            printf("Got mode %u = %x: %ux%ux%u\n", 
                vbe_num_modes, (uint16_t) ctx.regs.ecx, mode->width, mode->height, mode->bits_per_pixel);
            vbe_num_modes++;
            break;

        case 3: /* choose a mode */
            ctx.regs.eax = 0x101;
            break;
        }

        ThrSetV86Context(&ctx);
        ThrContinueV86();
    }
    else
    {
        /* This shouldn't happen because all relevant BIOS stuff gets handled in the kernel */
        fprintf(stderr, "VbeV86Handler: BIOS executed illegal thingy at %04lx:%04lx\n",
            ctx.cs, ctx.eip);
        vbe_failed = true;
        ThrExitThread(0);
    }
}

int main(int argc, char **argv)
{
    uint8_t *stack;
    FARPTR fp_code, fp_stackend;
    handle_t thr;
    const void *rsrc;
    size_t size;
    
    rsrc = ResFindResource(NULL, 256, 1, 0);
    if (rsrc == NULL)
    {
        printf("V86 code resource not found\n");
        return EXIT_FAILURE;
    }

    size = ResSizeOfResource(NULL, 256, 1, 0);
    printf("Code is %u bytes\n", size);
    fflush(stdout);

    vbe_code = VmmAlloc(PAGE_ALIGN_UP(size) / PAGE_SIZE, NULL, VM_MEM_READ | VM_MEM_WRITE);
    *(uint16_t*) vbe_code = 0x20cd;

    memcpy(vbe_code + 0x100, rsrc, size);
    
    fp_code = i386LinearToFp(vbe_code);
    fp_code = MK_FP(FP_SEG(fp_code), FP_OFF(fp_code) + 0x100);
	
    stack = VmmAlloc(PAGE_ALIGN_UP(65536) / PAGE_SIZE, NULL, VM_MEM_READ | VM_MEM_WRITE);
    fp_stackend = i386LinearToFp(stack);
    memset(stack, 0, 65536);
    
    thr = ThrCreateV86Thread(fp_code, fp_stackend, 15, VbeV86Handler);
    ThrWaitHandle(thr);

    if (!vbe_failed)
    {
        uint8_t *ptr = (uint8_t*) 0xa0000;
        unsigned i;

        for (i = 0; i < 65536; i++)
            ptr[i] = i;
    }

    /* xxx - need to clean up HndClose() implementation before we can use this */
    /*HndClose(thr);*/

    return EXIT_SUCCESS;
}
