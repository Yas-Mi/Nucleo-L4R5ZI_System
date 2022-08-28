@echo off

set BIN_PATH="C:\Program Files (x86)\Atollic\TrueSTUDIO for STM32 9.3.0\ARMTools\bin"
set GCC="arm-atollic-eabi-gcc.exe"
set OBJDUMP="arm-atollic-eabi-objdump.exe"
set OBJCOPY="arm-atollic-eabi-objcopy.exe"

%BIN_PATH%\%OBJCOPY% -O srec .\System.elf .\System.srec
