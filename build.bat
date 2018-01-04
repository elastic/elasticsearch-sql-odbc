REM This is just a helper script for building the ODBC driver in development.

setlocal EnableExtensions EnableDelayedExpansion
cls

SET ARG="%1"

if "%SRC_PATH%" == "" (
	SET SRC_PATH=\RnD\elastic\odbc
)
if "%BUILD_DIR%" == "" (
	SET BUILD_DIR=%USERPROFILE%\%SRC_PATH%\cbuild
)
if "%LOGGING_DIR%" == "" (
	SET LOGGING_DIR=%USERPROFILE%\Temp\
)
if "%INSTALL_DIR%" == "" (
	REM change to lib
	SET INSTALL_DIR=%USERPROFILE%\Temp\
)

pushd .
cd %BUILD_DIR%

REM presence of 1: 'clear' before building
if not _%ARG:1=% == _%ARG% (
	echo Cleaning all build files
	:: rm -r *
	del /s /q *
)

REM absence of 2: build
if _%ARG:2=% == _%ARG% (
	echo Generate the MSVC .sln project and build
	cmake ..
	MSBuild .\ALL_BUILD.vcxproj /t:rebuild
)

REM absence of 3: copy
if _%ARG:3=% == _%ARG% (
	echo Dump symbols and copy
	dumpbin /exports .\Debug\elasticodbc*.dll
	if "%VSCMD_ARG_TGT_ARCH%" == "x64" (
		SET PSIZE=
	) else (
		SET PSIZE=32
	)
	copy .\Debug\elasticodbc*.dll %INSTALL_DIR%\
)

REM absence of 4: clean logs
if _%ARG:4=% == _%ARG% (
	echo Clearing all logs
	echo.>%LOGGING_DIR%\mylog.txt
	echo.>%LOGGING_DIR%\SQL32.LOG
	echo.>%LOGGING_DIR%\SQL.LOG
)

popd
endlocal
