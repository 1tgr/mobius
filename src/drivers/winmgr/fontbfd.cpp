/*****************************************************************************
load BDF bitmap font into memory
These fonts used to be used (still are?) by X windows,
and are usually freely available.

xxx - bdfLoadFont() always allocates space for 256 characters
*****************************************************************************/
#include <string.h> /* strlen(), memcmp(), memset() */
#include <stdlib.h> /* atoi() */
/* FILE, printf(), sscanf(), fopen(), fgets(), fclose() */
#include <stdio.h>
#include <gui/font.h>
#include <os/stream.h>
#include <os/os.h>

class BfdFont : public IFont
{
protected:
	int Wd, Ht;
	int First, Last;
	byte *Spacings, *Bitmaps;

	int loadHelp(IStream *Infile, char *Line);

public:
	BfdFont();
	virtual ~BfdFont();
	int load(const wchar_t* FileName);

	STDMETHOD(DrawText)(ISurface* pSurf, int x, int y, const wchar_t* String,
		pixel_t pixColour);
	STDMETHOD(GetTextExtent)(const wchar_t* String, point_t* size);
};

IFont* CreateFontBfd(const wchar_t* lpszFileName)
{
	BfdFont* pFont = new BfdFont;
	if (pFont && pFont->load(lpszFileName) == 0)
		return pFont;
	else
	{
		delete pFont;
		return NULL;
	}
}

#define MAX_LINE_LEN    256
#define MAX_ENC     2048
#define MAX_WD      5
#define MAX_HT      40
/*****************************************************************************
*****************************************************************************/
BfdFont::BfdFont()
{
	Wd=0;
	Ht=0;
	First=0;
	Last=0;
	Spacings=NULL;
	Bitmaps=NULL;
}
/*****************************************************************************
*****************************************************************************/
BfdFont::~BfdFont(void)
{
	Wd=0;
	Ht=0;
	First=0;
	Last=0;
	if (Spacings != NULL)
	{
		delete[] Spacings;
		Spacings=NULL;
	}

	if (Bitmaps != NULL)
	{
		delete[] Bitmaps;
		Bitmaps=NULL;
	}
}

char* fgets(char* buf, int max, IStream* strm)
{
	char ch, *start = buf;

	while (true)
	{
		if (FAILED(IStream_Read(strm, &ch, 1)))
			return NULL;

		if (ch != '\n')
			*buf++ = ch;
		else
			break;
	}

	return start;
}

void fclose(IStream* strm)
{
	IUnknown_Release(strm);
}

IStream *_wfopen(const wchar_t *filename, const wchar_t *mode)
{
	IUnknown* ptr = sysOpen(filename);
	IStream* ret;

	if (ptr)
	{
		ret = NULL;
		IUnknown_QueryInterface(ptr, IID_IStream, (void**) &ret);
		IUnknown_Release(ptr);
		return ret;
	}
	else
		return NULL;
}

/*****************************************************************************
*****************************************************************************/
static int _expect(IStream* Infile, char *Line, char *Target, int Quiet)
{   unsigned Len;

	Len = strlen(Target);
	do
	{
		if (fgets(Line, MAX_LINE_LEN, Infile) == NULL)
		{
			fclose(Infile);
			if(!Quiet)
				wprintf(L"did not find %s line\n", Target);
			return(-1);
		}
	} while(memcmp(Line, Target, Len) != 0);

	return 0;
}
/*****************************************************************************
*****************************************************************************/
int Encoding=-1;
int FontYOff;

int BfdFont::loadHelp(IStream *Infile, char *Line)
{
	int CharWd, CharHt, CharXOff, CharYOff;//, Encoding;

	//  Encoding=atoi(Line + 9);
	Encoding++;
	if(Encoding > MAX_ENC)
	{
		wprintf(L"bad ENCODING (%u)\n", Encoding);
		return(-1);
	}

	if(Encoding < First)
		First=Encoding;
	if(Encoding > Last)
		Last=Encoding;
/* look for BBX, which gives the proportional spacing value (char width) */
	if(_expect(Infile, Line, "BBX", 0) != 0)
		return(-1);
	sscanf(Line + 4, "%d %d %d %d",
		&CharWd, &CharHt, &CharXOff, &CharYOff);
	Spacings[Encoding]=CharWd;
/* look for BITMAP */
	if(_expect(Infile, Line, "BITMAP", 0) != 0)
		return(-1);
/* start reading the actual raster data */
	{
		byte CharData[MAX_HT][MAX_WD], *Ptr;
		int Row, Col, YPos;

		memset(CharData, 0, sizeof(CharData));
		for(Row=0; Row < MAX_HT; Row++)
		{
			memset(Line, 0, MAX_LINE_LEN);
			if(fgets(Line, MAX_LINE_LEN, Infile) == NULL)
				return(-1);
/* exit this loop when ENDCHAR seen */
			if(memcmp(Line, "ENDCHAR", 7) == 0)
				break;
/* read char from top to bottom */
			YPos=Ht - CharHt +
				FontYOff - CharYOff + Row;
			if(YPos < 0 || YPos >= MAX_HT)
			{
				wprintf(L"warning: top or bottom edge "
					L"of char %d might be clipped (YPos "
					L"== %d)\n", Encoding, YPos);
				continue;
			}

			for(Col=0; Col < MAX_WD; Col++)
			{
				byte Temp;

				if(sscanf(Line + Col * 2, "%2x", &Temp) != 1)
					break;
				CharData[YPos][Col]=Temp; }}
/* store at Bitmaps */
		Ptr=Bitmaps + Wd * Ht * Encoding;
		for(Row=0; Row < Ht; Row++)
		{   for(Col=0; Col < Wd; Col++)
				Ptr[Col]=CharData[Row][Col];
			Ptr += Wd; }}
	return(0);
}

/*****************************************************************************
*****************************************************************************/
int BfdFont::load(const wchar_t* FileName)
{
	char Line[MAX_LINE_LEN];
	IStream* Infile;
//  int FontYOff;

	wprintf(L"bdfLoadFont '%s': ", FileName);
/* open file */
	Infile=_wfopen(FileName, L"r");
	if(Infile == NULL)
	{   wprintf(L"can't open file\n");
		return(-1); }
/* look for STARTFONT */
	if(_expect(Infile, Line, "STARTFONT", 0) != 0)
	{   fclose(Infile);
		return(-1); }
/* OK, it's a BDF font file: look for FONTBOUNDINGBOX */
	if(_expect(Infile, Line, "FONTBOUNDINGBOX", 0) != 0)
	{   fclose(Infile);
		return(-1); }

	{   int FontWd, FontXOff;

		sscanf(Line + 16, "%d %d %d %d",
			&FontWd, &Ht, &FontXOff, &FontYOff);
/* use FONTBOUNDINGBOX to set Wd (in range 1-MAX_WD bytes),
Ht (in range 1-MAX_HT rows), and FontYOff */
		Wd=(FontWd + 7) >> 3;
		if(Wd < 1 || Wd > MAX_WD)
		{   wprintf(L"bad Wd (%u)\n", Wd);
			fclose(Infile);
			return(-1); }
		if(Ht < 1 || Ht > MAX_HT)
		{   wprintf(L"bad Ht (%u)\n", Ht);
			fclose(Infile);
			return(-1); }
		wprintf(L"Wd=%u, Ht=%u, ",
			Wd, Ht);
	}
/* look for "CHARS ". You need the space,
or it will trip on CHARSET_REGISTRY, etc. */
	if(_expect(Infile, Line, "CHARS ", 0) != 0)
	{   fclose(Infile);
		return(-1); }
/* CHARS sets size of Spacings and Bitmaps arrays */
	{   unsigned NumChars;

		NumChars=MAX_ENC + 1;//atoi(Line + 6);  xxx
		Spacings=(byte *)malloc(NumChars);
		if(Spacings == NULL)
ERR_MEM:    {   wprintf(L"out of memory\n");
			fclose(Infile);
			return(-1); }
		Bitmaps=(byte *)malloc(NumChars * Ht * Wd);
		if(Bitmaps == NULL)
		{   free(Spacings);
			goto ERR_MEM; }
		wprintf(L"%u characters, ", NumChars); }
/* got all the info we need, move on to bitmap data */
	First=MAX_ENC;/* xxx */
	Last=0;   /* xxx */
/* look for ENCODING */
	{   int Quiet=0, Err=-1;

Encoding=31;
		while(_expect(Infile, Line, "ENCODING", Quiet) == 0)
		{   Err=loadHelp(Infile, Line);
			if(Err != 0)
				break;
			Quiet=1; }
First=0; /* xxx */
		fclose(Infile);
		if(Err != 0)
		{   free(Spacings);
			free(Bitmaps); }
		else wprintf(L"OK\n");
		return(Err); }}

HRESULT BfdFont::DrawText(ISurface* pSurf, int x, int y, const wchar_t* String, pixel_t pixColour)
{
	bmp1 SrcBmp;
	rect Clip;
	char Char;

	for(Char=*String++; Char != '\0'; Char=*String++)
/* valid Char? */
	{
		if(Char < TheFont.First || Char > TheFont.Last)
			continue;
		Char -= TheFont.First;
		if(TheFont.Spacings == NULL)
/* monospaced font: make each char 8 pixels * character cell width */
			Clip.Wd=TheFont.Wd * 8;
		else
			Clip.Wd=TheFont.Spacings[Char];
/* clip it */
		Clip.Ht=TheFont.Ht;
		Clip.DstX=this->CsrX;
		Clip.DstY=this->CsrY;
		if(!taliWinClip(&(Clip.SrcX), &(Clip.SrcY),
			&(Clip.DstX), &(Clip.DstY),
			&(Clip.Wd), &(Clip.Ht), this->Wd, this->Ht))
				continue;
/* create monochrome source bitmap */
		SrcBmp.Wd=TheFont.Wd * 8;
		SrcBmp.Ht=Clip.Ht;
		SrcBmp.Raster=TheFont.Bitmaps + TheFont.Wd *
			TheFont.Ht * Char;
/* blit it */
		this->Bitmap->blit1(Clip, SrcBmp);
/* advance cursor. ONE pixel between chars */
		this->CsrX=this->CsrX + Clip.Wd + 1;
	}
/* prevent delete[]/free() by SrcBmp destructor */
	SrcBmp.Raster=NULL;
}

/*****************************************************************************
*****************************************************************************/
HRESULT BfdFont::GetTextExtent(const wchar_t *String, point_t* size)
{
	unsigned RetVal;

	/* monospaced font */
	if(Spacings == NULL)
		return Point(strlen(String) * Wd * 8, Ht);

	/* proportional-spaced font */
	for(RetVal=0; *String != '\0'; String++)
	{
		if(*String < First || *String > Last)
			continue; /* undefined chars have zero width */
		RetVal += Spacings[*String - First] + 1;
	}

	size->x = RetVal;
	size->y = Ht;
	return S_OK;
}
