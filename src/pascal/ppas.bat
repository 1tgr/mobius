@echo off
SET THEFILE=system
echo Assembling %THEFILE%
nasm.exe -f obj -o system.o1 system.s1
if errorlevel 1 goto asmend
goto end
:asmend
echo An error occured while assembling %THEFILE%
goto end
:linkend
echo An error occured while linking %THEFILE%
:end
