/* $Id: es1371.h,v 1.1.1.1 2002/12/21 09:49:02 pavlovskii Exp $ */

#ifndef ES1371_H__
#define ES1371_H__

/*
 * Interrupt/Chip Select Control Register
 */

#define ES1371_REG_ICS_CONTROL  0x00

//                          0x0cdf0000
//                          0xfcdf0000
#define ICS_CONTROL_DEFAULT 0xfc0f0000  /* actually FCxF0000H */
#define ICS_PCICLKDIS       0x00000001  /* 1 = PCI clock disabled */
#define ICS_XTALCKDIS       0x00000002  /* 1 = xtal clock disabled */
#define ICS_JYSTK_EN        0x00000004  /* 1 = joystick enabled */
#define ICS_UART_EN         0x00000008  /* 1 = UART enabled */
#define ICS_ADC_EN          0x00000010  /* 1 = ADC record channel enabled */
#define ICS_DAC2_EN         0x00000020  /* 1 = DAC2 playback channel enabled */
#define ICS_DAC1_EN         0x00000040  /* 1 = DAC1 playback channel enabled */
#define ICS_BREQ            0x00000080  /* 1 = disable memory access */
#define ICS_PDLEV_D0        0x00000000  /* power-down level 0 */
#define ICS_PDLEV_D1        0x00000100  /* power-down level 1 */
#define ICS_PDLEV_D2        0x00000200  /* power-down level 2 */
#define ICS_PDLEV_D3        0x00000300  /* power-down level 3 */
#define ICS_CCB_INTRM       0x00000400  /* 1 = CCB voice interrupts enabled */
#define ICS_M_CB_ADC        0x00000000  /* CODEC ADC is record channel source */
#define ICS_M_CB_I2S        0x00000800  /* I2S is record channel source */
#define ICS_PWR_INTRM       0x00001000  /* 1 = power level change interrupts enabled */
#define ICS_ADC_STOP        0x00002000  /* 1 = CCB will not transfer record information */
#define ICS_SYNC_RES        0x00004000  /* generates Warm AC97 Reset as per 5.2.1.2, AC97 */
#define ICS_MSFMTSEL_SONY   0x00000000  /* MPEG serial data format Sony */
#define ICS_MSFMTSEL_I2S    0x00008000  /* MPEG serial data format I2S */
#define ICS_GPIO_OUT_MASK   0x000f0000  /* mask for writing to GPIO pins */
#define ICS_GPIO_IN_MASK    0x00f00000  /* mask for reading GPIO pins */
#define ICS_JOY_ASEL_200    0x00000000  /* joystick base address 200h */
#define ICS_JOY_ASEL_208    0x01000000  /* joystick base address 208h */
#define ICS_JOY_ASEL_210    0x02000000  /* joystick base address 210h */
#define ICS_JOY_ASEL_218    0x03000000  /* joystick base address 218h */

/*
 * Interrupt/Chip Select Status Register
 */

#define ES1371_REG_ICS_STATUS   0x04

//                          0x7f080ec0
#define ICS_STATUS_DEFAULT  0x7ffffec0
#define ICS_ADC             0x00000001  /* 1 = ADC interrupt pending */
#define ICS_DAC2            0x00000002  /* 1 = DAC2 interrupt pending */
#define ICS_DAC1            0x00000004  /* 1 = DAC1 interrupt pending */
#define ICS_UART            0x00000008  /* 1 = UART interrupt pending */
#define ICS_MCCB            0x00000010  /* 1 = CCB interrupt pending */
#define ICS_MPWR            0x00000020  /* 1 = power level interrupt pending */
#define ICS_VC_DAC1         0x00000000  /* CCB module voice code = DAC1 */
#define ICS_VC_DAC2         0x00000040  /* CCB module voice code = DAC2 */
#define ICS_VC_ADC          0x00000080  /* CCB module voice code = ADC */
#define ICS_VC_UNDEFINED    0x000000c0  /* CCB module voice code = undefined */
#define ICS_SYNC_ERR        0x00000100  /* 1 = CODEC sync. error has occurred */
#define ICS_INTR            0x80000000  /* 1 = interrupt pending */

/*
 * Memory page register
 */

#define ES1371_REG_PAGE         0x0c

#define BLOCK_DAC1          0x0         /* DAC1 sample bytes 0-63 */
#define BLOCK_DAC2          0x4         /* DAC2 sample bytes 0-63 */
#define BLOCK_ADC           0x8         /* ADC sample bytes 0-63 */
#define BLOCK_FRAME_UART    0xc         /* DAC/ADC frames, UART fifos */
#define PAGE_DAC_FRAME      0xc         /* DAC1/DAC2 frame information */
#define PAGE_ADC_FRAME      0xd         /* ADC frame information */
#define PAGE_UART_FIFO1     0xe         /* UART fifo */
#define PAGE_UART_FIFO2     0xf         /* UART fifo */

#define ES1371_REG_MEMORY       0x30    /* Host Interface - Memory */

#define ES1371_MEM_DAC1_FRAME_1 0x00
#define ES1371_MEM_DAC1_FRAME_2 0x04
#define ES1371_MEM_DAC2_FRAME_1 0x08
#define ES1371_MEM_DAC2_FRAME_2 0x0c

#define ES1371_MEM_ADC_FRAME_1  0x00
#define ES1371_MEM_ADC_FRAME_2  0x04

/*
 * Sample Rate Converter Interface Register
 */

#define ES1371_REG_SRC          0x10

//                          0x00470000
#define SRC_DEFAULT         0x00000000
#define SRC_RAM_DATA_MASK   0x0000ffff  /* value to be written to/read from SRC_RAM_ADR */
#define SRC_DIS_REC         0x00080000  /* 1 = record channel acc. update disabled */
#define SRC_DIS_P2          0x00100000  /* 1 = playback channel 1 acc. update disabled */
#define SRC_DIS_P1          0x00200000  /* 1 = playback channel 1 acc. update disabled */
#define SRC_DISABLE         0x00400000  /* 1 = SRC disabled */
#define SRC_RAM_BUSY        0x00800000  /* high when SRC is accessing RAM */
#define SRC_RAM_WE          0x01000000  /* read/write control bit for SRC RAM */
#define SRC_RAM_ADR_MASK    0xfe000000  /* address in SRC RAM to be accessed */
#define SRC_RAM_ADR_SHIFT   25

/* Sample rate converter registers */
#define SRC_REG_VOL_ADC_LEFT    0x6c
#define SRC_REG_VOL_ADC_RIGHT   0x6d
#define SRC_REG_DAC1            0x70
#define SRC_REG_DAC2            0x74
#define SRC_REG_ADC             0x78
#define SRC_REG_VOL_DAC1_LEFT   0x7c
#define SRC_REG_VOL_DAC1_RIGHT  0x7d
#define SRC_REG_VOL_DAC2_LEFT   0x7e
#define SRC_REG_VOL_DAC2_RIGHT  0x7f

#define SRC_REG_PLAY_TRUNC_N    0x00
#define SRC_REG_PLAY_INT_REGS   0x01
#define SRC_REG_PLAY_ACCUM_FRAC 0x02
#define SRC_REG_PLAY_VFREQ_FRAC 0x03

/*
 * CODEC Write Register
 */

#define ES1371_REG_CODEC_WRITE  0x14

#define CODEC_PIDAT_MASK        0x0000ffff      /* data */
#define CODEC_PIADD_MASK        0x007f0000      /* register address */
#define CODEC_PIADD_SHIFT       16
#define CODEC_PIRD              0x00800000      /* 0 = write, 1 = read */

/*
 * CODEC Read Register
 */

#define ES1371_REG_CODEC_READ   0x14

#define CODEC_PODAT_MASK        CODEC_PIDAT_MASK
#define CODEC_POADD_MASK        CODEC_PIADD_MASK
#define CODEC_POADD_SHIFT       CODEC_PIADD_SHIFT
#define CODEC_PORD              CODEC_PIRD
#define CODEC_WIP               0x40000000      /* 1 = register being accessed */
#define CODEC_RDY               0x80000000      /* 1 = data valid */

/*
 * Legacy Status/Control Register
 */

#define ES1371_REG_LEGACY       0x18

/*
 * Serial Interface Control Register
 */

#define ES1371_REG_SER_CONTROL  0x20

#define SER_P1_MB               0x00000001  /* 0 = DAC1 mono,  1 = DAC1 stereo */
#define SER_P1_EB               0x00000002  /* 0 = DAC1 8 bps, 1 = DAC1 16 bps */
#define SER_P2_MB               0x00000004  /* 0 = DAC2 mono,  1 = DAC2 stereo */
#define SER_P2_EB               0x00000008  /* 0 = DAC2 8 bps, 1 = DAC2 16 bps */
#define SER_R1_MB               0x00000010  /* 0 = ADC mono,   1 = ADC stereo */
#define SER_R1_EB               0x00000020  /* 0 = ADC 8 bps,  1 = ADC 16 bps */

#define SER_P1_FMT_SHIFT        0
#define SER_P2_FMT_SHIFT        2
#define SER_R1_FMT_SHIFT        4
#define SER_P2_DAC_SEN          0x00000040  /* 1 = DAC2 plays last sample when disabled/stopped */
#define SER_P1_SCT_RLD          0x00000080  /* forces reload of DAC1 sample counter on DAC1 clock */
#define SER_P1_INT_EN           0x00000100  /* 1 = DAC1 interrupt enabled */
#define SER_P2_INT_EN           0x00000200  /* 1 = DAC2 interrupt enabled */
#define SER_R1_INT_EN           0x00000400  /* 1 = ADC interrupt enabled */
#define SER_P1_PAUSE            0x00000800  /* 1 = playback 1 paused */
#define SER_P2_PAUSE            0x00001000  /* 1 = playback 2 paused */
#define SER_P1_LOOP_SEL         0x00002000  /* playback 1: 0 = loop mode, 1 = stop mode */
#define SER_P2_LOOP_SEL         0x00004000  /* playback 2: 0 = loop mode, 1 = stop mode */
#define SER_R1_LOOP_SEL         0x00008000  /* recording: 0 = loop mode, 1 = stop mode */
#define SER_P2_ST_INC_MASK      0x00070000  /* mask for restart sample address counter increment */
#define SER_P2_ST_INC_SHIFT     16
#define SER_P2_END_INC_MASK     0x00300000  /* mask for end sample address counter increment */
#define SER_P2_END_INC_SHIFT    19
#define SER_DAC_TEST            0x00400000  /* 1 = DAC test mode enabled */
#define SER_ONES                0xff800000

#define SER_FMT_STEREO          0x01
#define SER_FMT_16BIT           0x02

/*
 * DAC1 Sample Count Register
 */

#define ES1371_REG_DAC1_SAMPLE_COUNT    0x24

/*
 * DAC2 Sample Count Register
 */

#define ES1371_REG_DAC2_SAMPLE_COUNT    0x28

/*
 * ADC Sample Count Register
 */

#define ES1371_REG_ADC_SAMPLE_COUNT     0x2c

#endif
