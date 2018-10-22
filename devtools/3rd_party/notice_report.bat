@echo off
rem
rem Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
rem or more contributor license agreements. Licensed under the Elastic License;
rem you may not use this file except in compliance with the Elastic License.
rem
REM This is just a helper script for building the ODBC driver in development.

setlocal EnableExtensions EnableDelayedExpansion

set SRC_PATH=%~dp0
set OUT_FILE=%1

set LIC_DIR=%SRC_PATH%\licenses
set ES_NOTICE=%LIC_DIR%\..\..\..\NOTICE.txt

if [%OUT_FILE%] == [] (
	echo.
	echo USAGE: %~0 ^<output file^>
	echo.
	echo Assemble a "notice" file out of own NOTICE.txt and all 3rd party
	echo notices and licenses.
	exit /b 1
)

type %ES_NOTICE% > %OUT_FILE%

cd %LIC_DIR%
for /r %%i in ("*.txt") do if not %%~zi==0 (
	type %%i >> %OUT_FILE%
	echo.>>%OUT_FILE%
	echo.>>%OUT_FILE%
)

exit /b 0

endlocal
