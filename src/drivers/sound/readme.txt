================================================================
sndpkg
================================================================
Plays .WAV and .AU files in various formats (PCM, mu-law, A-law,
Microsoft ADPCM, and GSM) under DOS, using Windows Sound System
(WSS) sound board.

Chris Giese <geezer@execpc.com>, http://www.execpc.com/~geezer
Release date: Jan 3, 2001

You can do anything with this code but blame me for it or
call it your own.

Send E-mail if you like it, hate it, or want to help with these:

================================================================
BUGS/PLANS/TO DO
================================================================
showstoppers:
- put back DMA-probing code in wss.c
- wss.c: if you can't fill a buffer with data (i.e. at end of
  input file), fill it with zeroes (or do something else to
  make short files work with large buffers)
- include Win32 driver -OR- working SoundBlaster driver
	(these are more common sound devices than my WSS)
- pin DJGPP interrupt functions into memory

- test for DMA problems on machine with >16 meg RAM
- WSS softset doesn't work reliably
- can't probe board after running old version of play.exe
- finish/fix Sound Blaster driver
- restore Linux driver
- use formula (like mu-law) instead of table in alaw.c
- catch Ctrl-C and Ctrl-Break?

- documentation
- list key registers of Sound Blaster
- port wss.c and sb.c to NASM, implement as DOS device driver
  (SOUND$) TSR, that can be loaded/unloaded from command-line
- VOC files
- AIFF support, including AIFC with G.722 compression
- MP3
- CELP (old 28.8 Real Audio)
- utilities/special effects:
	reverb, reverse
	increase/decrease playback speed
	increase/decrease/maximize volume
	mix sound files
	equalizer/tone control/filtering
	DTMF (TouchTone) detection
- add ability to write files in these formats, as well as read them
- Linux: shared library codecs, loadable at run-time?
	good for proprietary/NDA codecs like TrueSpeech
        (does xanim already do this?)
- write X11 clone of Win95 sound recorder
- test on big endian machine

================================================================
SOURCES
================================================================
Voxware sound driver for Linux, by Hannu Savolainen
(hannu@voxware.pp.fi), for info on softsetting WSS DMA and IRQ
and providing initial working code.

Voxware sound driver for Linux, by Michael Schlueter
(michael@duck.syd.de) and Michael Marte
(marte@informatik.uni-muenchen.de) for providing A-law decoding
table.

DMA code came from Intel 8237 data sheet and some public-domain
code that I got from Simtel.

AD1848 data sheet in .PDF format. It used to be on Analog
Devices web site (www.analog.com) but no longer. I still have a
copy; E-mail me if you want it.

Marc Brevoort (m.r.j.Brevoort@ipr.nl) helped me get the original
AD1848-based player off the ground. He has AD1848 code for DOS
in Pascal.

Sox and Chris Bagwell's Audio File Formats FAQ, for information
on WAV, au, and other files formats.

Toast; the freeware GSM encoder/decoder from Jutta Degener and
Carsten Bormann.

Allegro game library, for DMA code and for Antti Koskipaa's
Windows Sound System driver.

