static inline uint16_t s3ColourToPixel16(colour_t clr)
{
    uint8_t r, g, b;
    r = COLOUR_RED(clr);
    g = COLOUR_GREEN(clr);
    b = COLOUR_BLUE(clr);
    /* 5-6-5 R-G-B */
    return ((r >> 3) << 11) |
           ((g >> 2) << 5) |
           ((b >> 3) << 0);
}


static inline colour_t s3Pixel16ToColour(uint16_t pix)
{
    uint8_t r, g, b;
    r = (pix >> 11) << 3;
    g = (pix >> 5) << 2;
    b = (pix >> 0) << 3;
    /* 5-6-5 R-G-B */
    return MAKE_COLOUR(r, g, b);
}


static void s3PutPixel16(video_t *vid, int x, int y, colour_t clr)
{
    uint16_t pix;

#ifdef TRIO_ACCEL
    Trio_WaitIdle();
#endif

    pix = s3ColourToPixel16(clr);
    if (clr == 0xffffffff)
        ((uint16_t*) (TrioMem + y * video_mode.bytesPerLine))[x] = 
        ~((uint16_t*) (TrioMem + y * video_mode.bytesPerLine))[x];
    else
        ((uint16_t*) (TrioMem + y * video_mode.bytesPerLine))[x] = pix;
}

static colour_t s3GetPixel16(video_t *vid, int x, int y)
{
    uint16_t pix;
#ifdef TRIO_ACCEL
    Trio_WaitIdle();
#endif
    pix = ((uint16_t*) (TrioMem + y * video_mode.bytesPerLine))[x];
    return s3Pixel16ToColour(pix);
}

static void s3HLine16(video_t *vid, int x1, int x2, int y, colour_t clr)
{
    uint16_t *ptr, pix;

    if (x2 < x1)
        swap_int(&x1, &x2);

#ifdef TRIO_ACCEL
    Trio_WaitIdle();
#endif
    pix = s3ColourToPixel16(clr);
    if (clr == 0xffffffff)
        for (ptr = ((uint16_t*) (TrioMem + y * video_mode.bytesPerLine)) + x1;
            x1 < x2;
            x1++, ptr++)
            *ptr = ~*ptr;
    else
        for (ptr = ((uint16_t*) (TrioMem + y * video_mode.bytesPerLine)) + x1;
            x1 < x2;
            x1++, ptr++)
            *ptr = pix;
}

