@echo off
rd /q /s System
rd /q /s Mobius
:: rd /s Drives
md System
md System\Boot
md System\Devices
md System\Ports
md Mobius
:: md Drives
:: md Drives\cdrom0
:: md Drives\cdrom1
:: md Drives\cdrom2
:: md Drives\cdrom3
:: md Drives\floppy0
:: md Drives\volume0
:: md Drives\volume1
:: md Drives\volume2
:: md Drives\volume3
for %%i in (
	ata.drv
	british.kbd
	russian.kbd
	us.kbd
	fat.drv
	ext2.drv
	cdfs.drv
	fdc.drv
	keyboard.drv
	ps2mouse.drv
	video.drv
	pci.drv
	sermouse.drv
	vga.kll
	vesa.kll
) do copy /y %%i System\Boot\
for %%i in (
	gustavo.bmp
	libc.dll
	libsys.dll
	freetype.dll
	gui.dll
	console.exe
	shell.exe
	vidtest.exe
	hello.exe
	test.exe
	tetris.exe
	mobius.bmp
	progress.bmp
	veramono.ttf
	vera.ttf
	winmgr.exe
	wincli.exe
	desktop.exe
	gconsole.exe
	thrtest.exe
) do copy /y %%i Mobius\
tar -czf mobius.tar.gz System Mobius
