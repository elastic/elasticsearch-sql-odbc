REM This is just a helper script for building the ODBC driver in development.

setlocal EnableExtensions EnableDelayedExpansion
cls

SET ARG="%1"

REM
REM  List of variables that can be customized
REM

if "%SRC_PATH%" == "" (
	SET SRC_PATH=%~dp0
)
if "%BUILD_DIR%" == "" (
	SET BUILD_DIR=%SRC_PATH%\builds
)
if "%LOGGING_DIR%" == "" (
	if EXIST %USERPROFILE%\AppData\Local\Temp\ (
		SET LOGGING_DIR=%USERPROFILE%\AppData\Local\Temp\
	) else (
		SET LOGGING_DIR="%USERPROFILE%\Local Settings\Temp\"
	)
)
if "%INSTALL_DIR%" == "" (
	REM change to lib
	if EXIST %USERPROFILE%\AppData\Local\Temp\ (
		SET INSTALL_DIR=%USERPROFILE%\AppData\Local\Temp\
	) else (
		SET INSTALL_DIR="%USERPROFILE%\Local Settings\Temp\"
	)
)

if "%LIBCURL_PATH_BUILD%" == "" (
	SET LIBCURL_PATH_BUILD=%SRC_PATH%\libs\curl\builds\libcurl-vc-x64-release-dll-ipv6-sspi-winssl
)

if "%CMAKE_BIN_PATH%" == "" (
	SET CMAKE_BIN_PATH="C:\Program Files\CMake\bin"
)



REM
REM Perform the building steps
REM

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
	%CMAKE_BIN_PATH%\%cmake ..
	REM MSBuild .\ALL_BUILD.vcxproj /t:rebuild
	MSBuild .\ALL_BUILD.vcxproj
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
