/*****************************************************************************
GSM
sub-format 49 of .WAV format

MONO ONLY

The original code came from the Toast website, listed below.
Any bugs in this code are due to Giese.

The original code was full of macros that made my old compiler choke.
It was also hand-optimized: loops were unrolled, the 'register' keyword
was used a lot, and RPE_grid_positioning() used something like Duff's
device. My version leaves optimization to the compiler, which lets you make
the usual choice between size and speed (e.g. use the GCC -funroll-loops
option or not).

I did, however, pare down the widths of data types where possible e.g.
changed 'int's and 'uword's to 'unsigned char'. The code in main() that
deals with .WAV files and the code in gsmDecodeFrame() that unpacks the
data is original. It's probably not as efficient as it could be, but I
can't seem to get DJGPP gprof working. I also took out the optional
floating-point code in long_term.c and short_term.c -- this code uses
mostly 16-bit integer math, with a little 32-bit when adding, subtracting,
or multiplying 16-bit values.


Source: http://kbs.cs.tu-berlin.de/~jutta/toast.html

Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann,
Technische Universitaet Berlin

Any use of this software is permitted provided that this notice is not
removed and that neither the authors nor the Technische Universitaet Berlin
are deemed to have made any representations as to the suitability of this
software for any purpose nor are held responsible for any defects of
this software.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.

As a matter of courtesy, the authors request to be informed about uses
this software has found, about bugs in this software, and about any
improvements that may be of general interest.

Berlin, 28.11.1994
Jutta Degener
Carsten Bormann
*****************************************************************************/
#include <stdlib.h> /* calloc() */
#include "sound.h"

/* -32768 isn't reall long, just put 'L' there to shut Turbo C up */
#define	MIN_WORD	-32768L
#define	MAX_WORD	32767

#define	SASR(x, by)	((x) >> (by))
/* short a, short b, !(a == b == MIN_WORD) */
#define GSM_MULT_R(a,b)	(SASR(((long)(a) * (long)(b) + 16384), 15))

#define	SATURATE(x) 	\
	((x) < MIN_WORD ? MIN_WORD : (x) > MAX_WORD ? MAX_WORD: (x))

#define	GSM_ADD(a, b)	\
	(SATURATE((long)(a) + (long)(b)))

#define	GSM_SUB(a, b)	\
	(SATURATE((long)(a) - (long)(b)))

#define GSM_ABS(a)	((a) < 0 ? ((a) == MIN_WORD ? MAX_WORD : -(a)) : (a))

typedef struct
{
	short	dp0[280];
	short	z1;		/* preprocessing.c, Offset_com. */
	long	L_z2;		/*                  Offset_com. */
	int	mp;		/*                  Preemphasis	*/
	short	u[8];		/* short_term_aly_filter.c	*/
	short	larpp[2][8]; 	/*                              */
	short	j;		/*                              */
	short	ltp_cut;        /* long_term.c, LTP crosscorr.  */
	short	nrp; /* 40 */	/* long_term.c, synthesis	*/
	short	v[9];		/* short_term.c, synthesis	*/
	short	msr;		/* decoder.c,	Postprocessing	*/

	unsigned char gsm_frame[65];
	unsigned short bit_pos;
	unsigned short samples_left;
	short output[320];
} gsm_decoder_t;
/*****************************************************************************
reads val_width-bit value from position bit_pos in bit_array
val_width <= 8

xxx - still fiddling with this. It gets called 76 times per GSM frame,
so it needs to be fast. 'gcc -O3' will probably inline it automatically,
but the puny compilers for 8- and 16-bit CPUs may not be so clever.
*****************************************************************************/
unsigned char read_bits(unsigned char *bit_array, unsigned short *bit_pos,
		unsigned char val_width)
{
	static const unsigned char mask_tab[] =
	{
		0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF
	};
	unsigned short byte_pos, temp;
	unsigned char bit_in_byte;

	byte_pos = (*bit_pos >> 3);
	bit_in_byte = (*bit_pos & 7);
/* the value we read may straddle two bytes,
so read 16-bit value (2 bytes) from bit_pos */
	temp = *(unsigned short *)(bit_array + byte_pos);
/* shift right to get the bits of interest at the bottom of temp */
	temp >>= bit_in_byte;
/* update bit_pos */
	*bit_pos += val_width;
/* mask away the upper bits */
	temp &= mask_tab[val_width];
/* return the bottom byte */
	return temp;
}
/*****************************************************************************
*****************************************************************************/
static short gsm_asr(short a, int n)
{
	if(n >= 16)
		return -(a < 0);
	if(n >= 0)
/* this needs to be an arithmetic shift right (i.e. with sign) */
		return a >> n;
	if(n < 0)
		return a << -n;
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static short gsm_asl(short a, int n)
{
	if(n >= 16)
		return 0;
	if(n <= -16)
		return -(a < 0);
	if(n < 0)
		return gsm_asr(a, -n);
	return a << n;
}
/****************************************************************************
SHORT TERM ANALYSIS FILTERING SECTION
4.2.8

larc[0-7]:	coded log area ratio
larpp[]:	out...
****************************************************************************/
static void decoding_of_the_coded_log_area_ratios(unsigned char *larc,
		short *larpp)
{
	static const short b_tab[] =
	{
		0, 0, 2048, -2560, 94, -1792, -341, -1144
	};
/* MIC[1..8]  = minimum value of the larc[1..8] */
	static const short mic_tab[] =
	{
		-32, -32, -16, -16, -8, -8, -4, -4
	};
/* INVA[1..8] = integer((32768 * 8) / real_A[1..8]) */
	static const short inva_tab[] =
	{
		13107, 13107, 13107, 13107, 19223, 17476, 31454, 29708
	};
	unsigned char i;
	short temp1;

	for(i = 0; i < 8; i++)
	{
		short temp2;

		temp1 = GSM_ADD(*larc, mic_tab[i]) << 10;
		temp2 = b_tab[i] << 1;
		temp1 = GSM_SUB(temp1, temp2);
		temp1 = GSM_MULT_R(inva_tab[i], temp1);
		*larpp = GSM_ADD(temp1, temp1);
		larc++;
		larpp++;
	}
}
/****************************************************************************
4.2.9
Computation of the quantized reflection coefficients

4.2.9.1  Interpolation of the larpp[1..8] to get the larp[1..8]

Within each frame of 160 analyzed speech samples the short term
analysis and synthesis filters operate with four different sets of
coefficients, derived from the previous set of decoded LARs(larpp(j-1))
and the actual set of decoded LARs (larpp(j))

Initial value: larpp(j-1)[1..8] = 0.)
****************************************************************************/
static void coefficients_0_12(short *larpp_j_1, short *larpp_j, short *larp)
{
	unsigned char i;

	for(i = 8; i != 0; i--)
	{
		*larp = GSM_ADD(SASR(*larpp_j_1, 2), SASR(*larpp_j, 2));
		*larp = GSM_ADD(*larp, SASR(*larpp_j_1, 1));
		larp++;
		larpp_j_1++;
		larpp_j++;
	}
}
/****************************************************************************
****************************************************************************/
static void coefficients_13_26(short *larpp_j_1, short *larpp_j, short *larp)
{
	unsigned char i;

	for(i = 8; i != 0; i--)
	{
		*larp = GSM_ADD(SASR(*larpp_j_1, 1), SASR(*larpp_j, 1));
		larpp_j_1++;
		larpp_j++;
		larp++;
	}
}
/****************************************************************************
****************************************************************************/
static void coefficients_27_39(short *larpp_j_1, short *larpp_j, short *larp)
{
	unsigned char i;

	for(i = 8; i != 0; i--)
	{
		*larp = GSM_ADD(SASR(*larpp_j_1, 2), SASR(*larpp_j, 2));
		*larp = GSM_ADD(*larp, SASR(*larpp_j, 1));
		larpp_j_1++;
		larpp_j++;
		larp++;
	}
}
/****************************************************************************
****************************************************************************/
static void coefficients_40_159(short *larpp_j, short *larp)
{
	unsigned char i;

	for(i = 8; i != 0; i--)
	{
		*larp = *larpp_j;
		larp++;
		larpp_j++;
	}
}
/****************************************************************************
4.2.9.2

larp[0-7]:	IN/OUT

The input of this procedure is the interpolated larp[0..7] array.
The reflection coefficients, rp[i], are used in the analysis
filter and in the synthesis filter.
****************************************************************************/
static void larp_to_rp(short *larp)
{
	short temp;
	int i;

	for(i = 8; i != 0; i--)
	{
		temp = GSM_ABS(*larp);
		if(temp < 11059)
			temp <<= 1;
		else if(temp < 20070)
			temp += 11059;
		else
			temp = GSM_ADD(temp >> 2, 26112);

		*larp = *larp < 0 ? -temp : temp;
		larp++;
	}
}
/****************************************************************************
rrp[0..7]:	IN
k:		k_end - k_start
wt[0..k-1]:	IN
sr[0..k-1]:	OUT
****************************************************************************/
static void short_term_synthesis_filtering(gsm_decoder_t *state,
		short *rrp, int k, short *wt, short *sr)
{
	short sri, tmp1, tmp2;
	short *v;
	int i;

	v = state->v;
	for(; k != 0; k--)
	{
		sri = *wt++;
		for(i = 8; i--; )
		{
			/* sri = GSM_SUB(sri, gsm_mult_r(rrp[i], v[i])); */
			tmp1 = rrp[i];
			tmp2 = v[i];
			tmp2 = (tmp1 == MIN_WORD && tmp2 == MIN_WORD
				? MAX_WORD
				: 0x0FFFF & (((long)tmp1 * (long)tmp2
					     + 16384) >> 15)) ;
			sri = GSM_SUB(sri, tmp2);
			/* v[i+1] = GSM_ADD(v[i], gsm_mult_r(rrp[i], sri));
			 */
			tmp1 = (tmp1 == MIN_WORD && sri == MIN_WORD
				? MAX_WORD
				: 0x0FFFF & (((long)tmp1 * (long)sri
					     + 16384) >> 15)) ;
			v[i+1] = GSM_ADD(v[i], tmp1);
		}
		*sr++ = v[0] = sri;
	}
}
/****************************************************************************
wt[0..159]:	received d	IN
s[0..159]:	signal		OUT
****************************************************************************/
static void gsm_short_term_synthesis_filter(gsm_decoder_t *state,
		unsigned char *larcr, short *wt, short *s)
{
	short *larpp_j, *larpp_j_1, larp[8];

	larpp_j = state->larpp[state->j];
	state->j ^= 1;
	larpp_j_1 = state->larpp[state->j];
	decoding_of_the_coded_log_area_ratios(larcr, larpp_j);

	coefficients_0_12(larpp_j_1, larpp_j, larp);
	larp_to_rp(larp);
	short_term_synthesis_filtering(state, larp, 13, wt, s);

	coefficients_13_26(larpp_j_1, larpp_j, larp);
	larp_to_rp(larp);
	short_term_synthesis_filtering(state, larp, 14, wt + 13, s + 13);

	coefficients_27_39(larpp_j_1, larpp_j, larp);
	larp_to_rp(larp);
	short_term_synthesis_filtering(state, larp, 13, wt + 27, s + 27);

	coefficients_40_159(larpp_j, larp);
	larp_to_rp(larp);
	short_term_synthesis_filtering(state, larp, 120, wt + 40, s + 40);
}
/*****************************************************************************
4.12.15

xmaxc:		IN
exp_out:	OUT
mant_aout:	OUT

Computes exponent and mantissa of the decoded version of xmaxc
*****************************************************************************/
static void apcm_quantization_xmaxc_to_exp_mant(short xmaxc, short *exp_out,
		short *mant_out)
{
	short exp, mant;

	exp = 0;
	if(xmaxc > 15)
		exp = SASR(xmaxc, 3) - 1;
	mant = xmaxc - (exp << 3);
	if(mant == 0)
	{
		exp = -4;
		mant = 7;
	}
	else
	{
		while(mant <= 7)
		{
			mant = (mant << 1) | 1;
			exp--;
		}
		mant -= 8;
	}
	*exp_out = exp;
	*mant_out = mant;
}
/*****************************************************************************
4.2.16

xmc[0..12]		IN
mant
exp
xMp[0..12]		OUT


This part is for decoding the RPE sequence of coded xmc[0..12]
samples to obtain the xmp[0..12] array.  Table 4.6 is used to get
the mantissa of xmaxc (FAC[0..7]).
*****************************************************************************/
static void apcm_inverse_quantization(unsigned char *xmc, short mant,
		short exp, short *xmp)
{
	static const short gsm_fac[8] =
	{
		18431, 20479, 22527, 24575, 26623, 28671, 30719, 32767
	};
	short temp, temp1, temp2, temp3;
	int i;

	temp1 = gsm_fac[mant];			/* see 4.2-15 for mant */
	temp2 = GSM_SUB(6, exp);		/* see 4.2-15 for exp */
	temp3 = gsm_asl(1, GSM_SUB(temp2, 1));
	for(i = 13; i != 0; i--)
	{
		temp = (*xmc++ << 1) - 7;		/* restore sign */
		temp <<= 12;			/* 16 bit signed */
		temp = GSM_MULT_R(temp1, temp);
		temp = GSM_ADD(temp, temp3);
		*xmp++ = gsm_asr(temp, temp2);
	}
}
/*****************************************************************************
4.2.17

Mc:		grid position	IN
xmp[0..12]:			IN
ep[0..39]:			OUT

This procedure computes the reconstructed long term residual signal
ep[0..39] for the LTP analysis filter.  The inputs are the mc
which is the grid position selection and the xmp[0..12] decoded
RPE samples which are upsampled by a factor of 3 by inserting zero
values.
*****************************************************************************/
static void rpe_grid_positioning(short mc, short *xmp, short *ep)
{
	int i, k;

	for(k = 0; k <= 39; k++)
		ep[k] = 0;
	for(i = 0; i <= 12; i++)
		ep[mc + 3 * i] = xmp[i];
}
/*****************************************************************************
4.3.2
Ncr:
bcr:
erp[0..39]:		IN
drp[-120..40]:		IN[-120..-1], OUT[-120..40]

This procedure uses the bcr and ncr parameter to realize the
long term synthesis filtering.  The decoding of bcr needs table 4.3b.
*****************************************************************************/
static void gsm_long_term_synthesis_filtering(gsm_decoder_t *state,
		unsigned char ncr, unsigned char bcr, short *erp, short *drp)
{
	static const short gsm_qlb[4] =
	{
		3277, 11469, 21299, 32767
	};
	short brp, drpp, nr;
	int k;

/* Check the limits of nr. */
	nr = ncr < 40 || ncr > 120 ? state->nrp : ncr;
	state->nrp = nr;
/* Decoding of the LTP gain bcr */
	brp = gsm_qlb[bcr];
/* Computation of the reconstructed short term residual signal drp[0..39] */
	for(k = 0; k <= 39; k++)
	{
		drpp = GSM_MULT_R(brp, drp[k - nr]);
		drp[k] = GSM_ADD(erp[k], drpp);
	}
/* Update of the reconstructed short term residual signal drp[ -1..-120 ] */
	for(k = 0; k <= 119; k++)
		drp[k - 120] = drp[k - 80];
}
/*****************************************************************************
*****************************************************************************/
static void gsm_decode_frame(gsm_decoder_t *state, unsigned char *src,
		short *dst)
{/* 260-bit GSM frame == 36 + 4 * (17 + 39) == 32.5 bytes
36 bits			larc */
	static const unsigned char larc_width_table[] =
	{
		6, 6, 5, 5, 4, 4, 3, 3
	};
/* these values are repeated four times */
	static const unsigned char width_table[] =
	{
/* 17 bits		Nc[0], bc[0], mc[0], xmaxc[0] */
		7, 2, 2, 6,
/* 39 bits (13x3)	xmc[0-12] */
		3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
	};
	unsigned char temp, parms[76], *parm_ptr, k;
	short erp[40], *drp, wt[160], tmp;
	const unsigned char *width;
/* read a frame from src and unpack it to parms:
	0-7	larc[0-7]

	8	Nc[0]		25	Nc[1]
	9	bc[0]		26	bc[1]
	10	mc[0]		27	mc[1]
	11	xmaxc[0]	28	xmaxc[1]
	12-24	xmc[0-12]	29-41	xmc[13-25]

	42	Nc[2]		59	Nc[3]
	43	bc[2]		60	bc[3]
	44	mc[2]		61	mc[3]
	45	xmaxc[2]	62	xmaxc[3]
	46-58	xmc[26-38]	63-75	xmc[39-51] */

	parm_ptr = parms;
	width = larc_width_table;
	for(temp = 8; temp != 0; temp--)
	{
		*parm_ptr = read_bits(src, &state->bit_pos, *width);
		parm_ptr++;
		width++;
	}
	for(temp = 4; temp != 0; temp--)
	{
		width = width_table;
		for(k = 17; k != 0; k--)
		{
			*parm_ptr = read_bits(src, &state->bit_pos, *width);
			parm_ptr++;
			width++;
		}
	}
	drp = state->dp0 + 120;
	parm_ptr = parms + 8;
	for(temp = 0; temp < 4; temp++)
	{
		short exp, mant, xmp[13];

		apcm_quantization_xmaxc_to_exp_mant(parm_ptr[3], &exp, &mant);
		apcm_inverse_quantization(parm_ptr + 4, mant, exp, xmp);
		rpe_grid_positioning(parm_ptr[2], xmp, erp);
		gsm_long_term_synthesis_filtering(state,
			parm_ptr[0],	/*  *Nc	*/
			parm_ptr[1],	/*  *bc	*/
			erp, drp);
		for(k = 0; k <= 39; k++)
			wt[temp * 40 + k] = drp[k];
		parm_ptr += 17;
	}
	gsm_short_term_synthesis_filter(state,
		parms,			/*  larc */
		wt, dst);
/* postprocessing */
	for(temp = 160; temp != 0; temp--)
	{
		tmp = GSM_MULT_R(state->msr, 28180 );
		state->msr = GSM_ADD(*dst, tmp); /* Deemphasis */
		*dst = GSM_ADD(state->msr, state->msr)
			& 0xFFF8;	/* Truncation & Upscaling */
		dst++;
	}
}
/*****************************************************************************
*****************************************************************************/
static int get_sample(sound_info_t *info, short *left, short *right)
{
	gsm_decoder_t *state;

	state = (gsm_decoder_t *)info->fsd;
/* need to load a new GSM frame? */
	if(state->samples_left == 0)
	{
/* read 65 bytes (two 32.5-byte GSM frames; Microsoft's packing method) */
		if(fread(state->gsm_frame, 1, 65, info->infile) != 65)
			return -1;
/* decode the frames */
		state->bit_pos = 0;
		gsm_decode_frame(state, state->gsm_frame, state->output);
		gsm_decode_frame(state, state->gsm_frame, state->output + 160);
		state->samples_left = 320;
	}
	*left = *right = state->output[320 - state->samples_left];
	state->samples_left--;
	return 0;
}
/*****************************************************************************
*****************************************************************************/
/* in WAV.C */
int wav_validate(sound_info_t *info, unsigned sub_type);

static int validate(sound_info_t *info)
{
	int err;

	fseek(info->infile, 0, SEEK_SET);
/* WAV sub-format #49 is GSM */
	err = wav_validate(info, 49);
	if(err == 0)
	{
		if(info->channels != 1)
		{
			printf("sorry, only MONO GSM supported\n");
			return -1;
		}
/* use calloc(), so it's zeroed */
		info->fsd = (void *)calloc(1, sizeof(gsm_decoder_t));
		if(info->fsd == NULL)
			return -1;
		info->depth = 16;
	}
	return err;
}
/*****************************************************************************
*****************************************************************************/
codec_t _wav49 =
{
	".wav Microsoft GSM", validate, get_sample
};
