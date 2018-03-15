REM This is just a helper script for building the ODBC driver in development.
@echo off

setlocal EnableExtensions EnableDelayedExpansion
cls

SET ARG="%*"

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
	if exist %USERPROFILE%\AppData\Local\Temp\ (
		SET LOGGING_DIR=%USERPROFILE%\AppData\Local\Temp\
	) else (
		SET LOGGING_DIR="%USERPROFILE%\Local Settings\Temp\"
	)
)
if "%INSTALL_DIR%" == "" (
	REM change to lib
	if exist %USERPROFILE%\AppData\Local\Temp\ (
		SET INSTALL_DIR=%USERPROFILE%\AppData\Local\Temp
	) else (
		SET INSTALL_DIR="%USERPROFILE%\Local Settings\Temp"
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

REM presence of 'help': output a usage message
if not _%ARG:help=% == _%ARG% (
	echo Usage: %0 [argument(s^)]
	echo.
	echo The following arguments are supported:
	echo       help      : output this message and exit.
	echo       clean     : clean the build dir files.
	echo       proper    : clean both the build and libs dir and exit.
	echo       fetch     : fetch, patch and build the dependency libs.
	echo       nobuild   : don't build the project (the default is to build^).
	echo       copy      : copy the DLL into the test dir.
	echo       regadd    : register the driver into the registry (run as Administrator^).
	echo       regdel    : deregister the driver from the registry (run as Administrator^).
	echo       trunclogs : truncate the logs.
	echo.
	echo Multiple arguments can be used concurrently.
	echo Invoked with no arguments, the script will only initiate a build.
	echo.
	
	goto end
)

REM presence of 'proper': clean up the build and libs dir before building
if not _%ARG:proper=% == _%ARG% (
	echo Cleaning all build and libs files.
	del /s /q %BUILD_DIR%\* >nul 2>&1
	for /d %%i in (%BUILD_DIR%\*) do rmdir /s /q %%i >nul 2>&1
	del /s /q libs\* >nul 2>&1
	for /d %%i in (libs\*) do rmdir /s /q %%i >nul 2>&1

	goto end
) else if not _%ARG:clean=% == _%ARG% (
	REM presence of 'clean': clean up the build dir before building
	echo Cleaning all build files.
	del /s /q %BUILD_DIR%\* >nul 2>&1
	for /d %%i in (%BUILD_DIR%\*) do rmdir /s /q %%i >nul 2>&1
)

REM presence of 'fetch': fetch, patch, build the external libs
if not _%ARG:fetch=% == _%ARG% (
	echo Fetching external dependencies, patching and building them.

	rem pushd .
	cd libs

	if not exist ODBC-Specification (
		git clone "https://github.com/Microsoft/ODBC-Specification.git"
	) else (
		echo ODBC-Specification dir present, skipping cloning repo.
	)
	if not exist c-timestamp (
		git clone "https://github.com/chansen/c-timestamp.git"
	) else (
		echo c-timestamp dir present, skipping cloning repo.
	)
	if not exist ujson4c (
		git clone "https://github.com/esnme/ujson4c.git"

		REM %cd% is expanded before execution and patch command will need full
		REM path, as it might be started in different working dir than current
		patch -p1 -i %cd%\ujson4c.diff -d %cd%\libs\ujson4c
	) else (
		echo ujson4c dir present, skipping cloning repo.
	)
	if not exist curl (
		git clone "https://github.com/curl/curl.git"
	) else (
		echo curl dir present, skipping cloning repo.
	)

	REM build libcurl
	cd curl
	git checkout curl-7_58_0
	
	call buildconf.bat
	REM buildconf.bat will cd
	cd winbuild
	call nmake /f Makefile.vc mode=dll MACHINE=x64
)


cd %BUILD_DIR%

REM absence of nobuild: run the build
if _%ARG:nobuild=% == _%ARG% (
	echo Generating the MSVC .sln for the project and building it.
	%CMAKE_BIN_PATH%\%cmake ..
	REM MSBuild .\ALL_BUILD.vcxproj /t:rebuild
	MSBuild .\ALL_BUILD.vcxproj
) else (
	echo Invoked with 'nobuild', building skipped.
)

REM presence of copy: copy DLLs (libcurl, odbc) to the test "install" dir
if not _%ARG:copy=% == _%ARG% (
	echo Dumping exported DLL symbols and copying into test install folder.
	rem dumpbin /exports .\Debug\elasticodbc*.dll

	copy .\Debug\elasticodbc*.dll %INSTALL_DIR%\
	copy %LIBCURL_PATH_BUILD%\bin\libcurl.dll %INSTALL_DIR%\
) else (
	REM Invoked without 'copy': DLLs test installation skipped.
)

REM presence of regadd: add driver into the registry
if not _%ARG:regadd=% == _%ARG% (
	echo Adding driver into the registry.

	REM check if driver exists, otherwise the filename is unknown
	if not exist .\Debug\elasticodbc*.dll (
		echo Error: Driver can only be added into the registry once built.
		goto end
	)
	for /f %%i in (".\Debug\elasticodbc*.dll") do set DRVNAME=%%~nxi

	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" ^
		/v "Elasticsearch ODBC" /t REG_SZ /d Installed /f /reg:64

	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /f /reg:64
	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /v Driver ^
		/t REG_SZ /d %INSTALL_DIR%\!DRVNAME! /f /reg:64
	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /v Setup ^
		/t REG_SZ /d %INSTALL_DIR%\!DRVNAME! /f /reg:64
	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /v UsageCount^
		/t REG_DWORD /d 1 /f /reg:64
) else (
	REM Invoked without 'regadd': registry adding skipped.
)

REM presence of regdel: remove driver from the registry
if not _%ARG:regdel=% == _%ARG% (
	echo Removing driver from the registry.

	reg delete "HKLM\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" ^
		/v "Elasticsearch ODBC" /f /reg:64
	reg delete "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /f /reg:64
) else (
	REM Invoked without 'regadd': registry adding skipped.
)

REM presence of keeplogs: empty logs files
if not _%ARG:trunclogs=% == _%ARG% (
	echo Truncating all logs.
	echo.>%LOGGING_DIR%\mylog.txt
	echo.>%LOGGING_DIR%\SQL32.LOG
	echo.>%LOGGING_DIR%\SQL.LOG
) else (
	REM Invoked without 'trunclogs', logs not truncated.
)

:end
endlocal
