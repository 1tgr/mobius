@echo off
set msdev=f:\program files\microsoft visual studio\vc98\bin;f:\program files\microsoft visual studio\common\msdev98\bin
set nasm=f:\drived\nasm
set bochs=c:\bochs
set gcc=f:\drived\cygnus\cygwin-b20\H-i586-cygwin32\bin
set fpc=c:\pp\bin\win32
rem path %systemroot%;%systemroot%\system32;%msdev%;%nasm%;%bochs%;%gcc%;%fpc%
rem d:\cygnus\cygwin-b20\H-i586-cygwin32\bin;
rem set djgpp=f:\drived\djgpp\djgpp.env
make %1 %2 %3 %4 %5 %6 %7 %8 %9
