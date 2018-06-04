REM Helper script to generate the definition file of symbols to export in DLL

@echo off

REM TODO: find out a smart way for this filtering
set FILTER="Select-String -Pattern 'SQLRETURN\s+SQL_API\s+\w+'  *.c | %%{ [regex]::split($_, '\s+')[2]; } | %%{ [regex]::split($_, '\(')[0]; }"

set DRV_NAME=%1
set OUTFILE=%2
if not defined DRV_NAME (set DRV_NAME=elasticodbc)
if not defined OUTFILE (set OUTFILE=%DRV_NAME%.def)

echo LIBRARY %DRV_NAME%.dll> %OUTFILE%
echo VERSION 0.2>> %OUTFILE%
echo.>> %OUTFILE%
echo EXPORTS>> %OUTFILE%
echo.>> %OUTFILE%
powershell -NoLogo -ExecutionPolicy Bypass -Command %FILTER%>> %OUTFILE%
echo.>> %OUTFILE%
REM minimize the exposure of this unwanted export
echo JSON_DecodeObject @1 NONAME PRIVATE>> %OUTFILE%
