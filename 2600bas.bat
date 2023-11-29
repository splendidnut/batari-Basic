@echo off
if X"%bB%" == X goto nobb
rem preprocess <"%~f1" | 2600basic.exe -i "%bB%" > bB.asm
batari-basic.exe -s "%~f1" -i "%bB%" > "%~f1.asm"
if errorlevel 1 goto bBerror

dasm "%~f1.asm" -I"%bB%"/includes -f3 -l"%~f1.lst" -s"%~f1.sym" -o"%~f1.bin"
rem | bbfilter

goto end

:nobb
echo bB environment variable not set.

:bBerror
echo Compilation failed.

:end
