#include <stdio.h> /* FILE */
#include <kernel/kernel.h>
#include <kernel/driver.h>

#define DEBUG
#include <kernel/debug.h>

/* these assumes sizeof(short)==2
and it assumes little-endian CPU (e.g. x86)
these functions used to read from sound files */
#define	read_le16(S)	*(unsigned short *)(S)
#define	read_le32(S)	*(unsigned long *)(S)
unsigned short read_be16(unsigned char *buffer);
unsigned long read_be32(unsigned char *buffer);

/* these functions used to write to sound hardware */
#define	write_le16(D,S)	*(unsigned short *)(D) = S

/* byte offsets in "fmt " block of RIFF/WAVE file */
#define	WAV_FMT_LEN	0	/* 32-bit actual length of "fmt " block */
#define	WAV_FMT_FMT	4	/* 16-bit compression type */
#define	WAV_FMT_CHAN	6	/* 16-bit mono/stereo */
#define	WAV_FMT_RATE	8	/* 32-bit sample rate */
#define	WAV_FMT_BPS	12	/* 32-bit ? */
#define	WAV_FMT_ALIGN	16	/* 16-bit ? */
#define	WAV_FMT_DEPTH	18	/* 16-bit bits/sample */
#define	WAV_FMT_SIZE	20	/* min. length of "fmt " block */

#if (defined(__TURBOC__) || !defined(__cplusplus)) && !defined(bool)
typedef enum
{
	false = 0, true = 1
} bool;
#endif

typedef struct sound_t sound_t;
struct sound_t
{
	bool	(*open)(sound_t *snd);
	long	(*find_closest_rate)(sound_t *snd, long rate);
	int		(*set_fmt)(sound_t *snd, unsigned char depth, 
		unsigned char channels, long rate);
	void	(*set_volume)(sound_t *snd, unsigned char level);
	int		(*write_sample)(sound_t *snd, short left, short right);
	void	(*stop_playback)(sound_t *snd, bool wait);
	void	(*close)(sound_t *snd);
	void	(*irq)(sound_t *snd, unsigned num);
};

typedef struct
{
	FILE *infile;
	unsigned long bytes_left;
	void *fsd;
	unsigned char channels;
	unsigned char depth;
	long rate;
} sound_info_t;

typedef struct
{
	const char *name;
	int (*validate)(sound_info_t *info);
	int (*get_sample)(sound_info_t *info, short *left, short *right);
} codec_t;
