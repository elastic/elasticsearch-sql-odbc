REM This is just a helper script for building the ODBC driver in development.
@echo off

setlocal EnableExtensions EnableDelayedExpansion
REM cls

SET ARG="%*"
SET SRC_PATH=%~dp0
call:SET_TEMP

REM 32/64 bit argument needs to stay on top, before setting LIBCURL path.
REM presence of '32' or '64': set the Bits/Target ARCHhitecture
if not _%ARG:32=% == _%ARG% (
	set TARCH=x86
	set BARCH=32
) else (
	set TARCH=x64
	set BARCH=64
)



REM
REM  List of variables that can be customized
REM

if "%BUILD_DIR%" == "" (
	SET BUILD_DIR=%SRC_PATH%\builds
)
if not "%ESODBC_LOG_DIR%" == "" (
	for /f "tokens=1 delims=?" %%a in ("%ESODBC_LOG_DIR%") do set LOGGING_DIR=%%a
)
if "%INSTALL_DIR%" == "" (
	set INSTALL_DIR=%TEMP%
)

if "%LIBCURL_PATH_BUILD%" == "" (
	set LIBCURL_PATH_BUILD=%SRC_PATH%\libs\curl\builds\libcurl-vc-!TARCH!-release-dll-ipv6-sspi-winssl
)

if "%CMAKE_BIN_PATH%" == "" (
	set CMAKE_BIN_PATH="C:\Program Files\CMake\bin"
)



REM
REM  Perform the building steps
REM

REM presence of 'help'/'?': invoke USAGE "function" and exit
if /i not _%ARG:help=% == _%ARG% (
	call:USAGE %0
	goto end
) else if not _%ARG:?=% == _%ARG% (
	call:USAGE %0
	goto end
)

REM presence of 'proper' or 'clean': invoke respective "functions"
if /i not _%ARG:proper=% == _%ARG% (
	call:PROPER
	goto end
) else if /i not _%ARG:clean=% == _%ARG% (
	call:CLEAN
)

REM presence of 'setup': invoke SETUP "function"
if /i not _%ARG:setup=% == _%ARG% (
	call:SETUP
) else (
	REM Invoked without 'setup': setting up build vars skipped.

	where cl.exe >nul 2>&1
	if ERRORLEVEL 1 (
		echo.
		echo Note: building environment not set. Run with /? to see options.
		echo.
	)
)
REM


REM presence of 'fetch': invoke FETCH "function"
if /i not _%ARG:fetch=% == _%ARG% (
	call:FETCH
)


cd %BUILD_DIR%

REM absence of nobuild: invoke BUILD "function"
if /i _%ARG:nobuild=% == _%ARG% (
	call:BUILD
) else (
	echo Invoked with 'nobuild', building skipped.
)

REM presence of 'copy': invoke COPY "function"
if /i not _%ARG:copy=% == _%ARG% (
	call:COPY
) else (
	REM Invoked without 'copy': DLLs test installation skipped.
)

REM presence of 'clearlogs': invoke CLEARLOGS "function"
if /i not _%ARG:clearlogs=% == _%ARG% (
	call:CLEARLOGS
) else (
	REM Invoked without 'clearlogs', logs not touched.
)

REM presence of 'regadd': call REGADD "function"
if /i not _%ARG:regadd=% == _%ARG% (
	call:REGADD
) else (
	REM Invoked without 'regadd': registry adding skipped.
)

REM presence of 'regdel': invoke REGDEL "function"
if /i not _%ARG:regdel=% == _%ARG% (
	call:REGDEL
) else (
	REM Invoked without 'regadd': registry adding skipped.
)

:end
exit /b 0



REM
REM  "Functions"
REM

REM if TEMP var not set, set it.
:SET_TEMP
	if exist %TEMP% goto:eof
	set TEMP=%TMP%
	if exist %TEMP% goto:eof
	set TEMP=%USERPROFILE%\AppData\Local\Temp
	if exist %TEMP% goto:eof
	set TEMP="%USERPROFILE%\Local Settings\Temp\"
	if exist %TEMP% goto:eof
	echo.
	echo WARNING: no temporary directory available; using root
	echo.
	set TEMP=\

	goto:eof


REM USAGE function: output a usage message
:USAGE
	echo Usage: %1 [argument(s^)]
	echo.
	echo The following arguments are supported:
	echo       help^|*?*  : output this message and exit.
	echo       32^|64     : setup the architecture to 32- or 64-bit (default^);
	echo                   useful with 'setup' and 'reg*' arguments only;
	echo                   x86 and x64 platforms supported only.
	echo       setup     : invoke MSVC's build environment setup script before
	echo                   building (requires 2017 version or later^).
	echo       clean     : clean the build dir files.
	echo       proper    : clean both the build and libs dir and exit.
	echo       fetch     : fetch, patch and build the dependency libs.
	echo       nobuild   : skip project building (the default is to build^).
	echo       copy      : copy the DLL into the test dir (%INSTALL_DIR%^).
	echo       regadd    : register the driver into the registry;
	echo                   (needs Administrator privileges^).
	echo       regdel    : deregister the driver from the registry;
	echo                   (needs Administrator privileges^).
	echo       clearlogs : clear the logs (in: %LOGGING_DIR%^).
	echo.
	echo Multiple arguments can be used concurrently.
	echo Invoked with no arguments, the script will only initiate a build.
	echo Example:^> %1 setup 32 fetch
	echo.
	echo List of settable environment variables:
	echo       BUILD_DIR          : path to folder to hold the build files;
	echo                            now set to: `%BUILD_DIR%`.
	echo       ESODBC_LOG_DIR     : path to folder holding the logging files;
	echo                            now set to: `%ESODBC_LOG_DIR%`.
	echo       INSTALL_DIR        : path to folder to hold the built driver;
	echo                            now set to: `%INSTALL_DIR%`.
	echo       LIBCURL_PATH_BUILD : path to libcurl library;
	echo                            now set to: `%LIBCURL_PATH_BUILD%`.
	echo       CMAKE_BIN_PATH     : path to cmake executable;
	echo                            now set to: `%CMAKE_BIN_PATH%`.
	echo.

	goto:eof


REM PROPER function: clean up the build and libs dir before building
:PROPER
	echo Cleaning all build and libs files.
	del /s /q %BUILD_DIR%\* >nul 2>&1
	for /d %%i in (%BUILD_DIR%\*) do rmdir /s /q %%i >nul 2>&1
	del /s /q libs\* >nul 2>&1
	for /d %%i in (libs\*) do rmdir /s /q %%i >nul 2>&1

	goto:eof


REM CLEAN function: clean up the build dir before building
:CLEAN
	echo Cleaning all build files.
	del /s /q %BUILD_DIR%\* >nul 2>&1
	for /d %%i in (%BUILD_DIR%\*) do rmdir /s /q %%i >nul 2>&1

	goto:eof


REM SETUP function: set-up the build environment
:SETUP
	set RELEASE=2017
	for %%e in (Enterprise, Professional, Community) do (
		if exist "C:\Program Files (x86)\Microsoft Visual Studio\%RELEASE%\%%e\Common7\Tools\VsDevCmd.bat" (
			if /i "%%e" == "Community" (
				echo.
				echo WARNING: Community edition is not licensed to build commerical projects.
				echo.
			)
			call "C:\Program Files (x86)\Microsoft Visual Studio\%RELEASE%\%%e\Common7\Tools\VsDevCmd.bat" -arch=!TARCH!
			set EDITION=%%e
			break
		)
	)
	if "%EDITION%" == "" (
		echo.
		echo WARNING: no MSVC edition found, environment not set.
		echo.
	)

	goto:eof


REM FETCH function: fetch, patch, build the external libs
:FETCH
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
	call nmake /f Makefile.vc mode=dll MACHINE=!TARCH!

	goto:eof


REM BUILD function: compile, build the driver
:BUILD
	echo Building the driver:
	if not exist ALL_BUILD.vcxproj (
		echo Generating the project files.
		%CMAKE_BIN_PATH%\%cmake ..
	)
	echo Building the project.
	REM MSBuild ALL_BUILD.vcxproj /t:rebuild
	MSBuild ALL_BUILD.vcxproj

	goto:eof


REM COPY function: copy DLLs (libcurl, odbc) to the test "install" dir
:COPY
	echo Copying into test install folder %INSTALL_DIR%.
	rem dumpbin /exports Debug\elasticodbc*.dll

	copy Debug\elasticodbc*.dll %INSTALL_DIR%\
	copy %LIBCURL_PATH_BUILD%\bin\libcurl.dll %INSTALL_DIR%\

	goto:eof


REM CLEARLOGS function: empty logs files
:CLEARLOGS
	if not "%LOGGING_DIR%" == "" (
		echo Clearing logs in %LOGGING_DIR%.
		del %LOGGING_DIR%\esodbc_*.log >nul 2>&1
		if exist %LOGGING_DIR%\SQL32.LOG (
			echo.>%LOGGING_DIR%\SQL32.LOG
		)
		if exist %LOGGING_DIR%\SQL.LOG (
			echo.>%LOGGING_DIR%\SQL.LOG
		)
	) else (
		echo No logging directory to clear set; re-run with 'help' argument.
	)
	goto:eof


REM REGADD function: add driver into the registry
:REGADD
	echo Adding driver into the registry.

	REM check if driver exists, otherwise the filename is unknown
	if not exist %BUILD_DIR%\Debug\elasticodbc*.dll (
		echo Error: Driver can only be added into the registry once built.
		goto end
	)
	for /f %%i in ("%BUILD_DIR%\Debug\elasticodbc*.dll") do set DRVNAME=%%~nxi

	echo Adding ESODBC driver %INSTALL_DIR%\!DRVNAME! to the registry.

	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" ^
		/v "Elasticsearch ODBC" /t REG_SZ /d Installed /f /reg:!BARCH!

	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /f ^
		/reg:!BARCH!
	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /v Driver ^
		/t REG_SZ /d %INSTALL_DIR%\!DRVNAME! /f /reg:!BARCH!
	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /v Setup ^
		/t REG_SZ /d %INSTALL_DIR%\!DRVNAME! /f /reg:!BARCH!
	reg add "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /v UsageCount^
		/t REG_DWORD /d 1 /f /reg:!BARCH!

	goto:eof


REM REGDEL function: remove driver from the registry
:REGDEL
	echo Removing ESODBC driver from the registry.

	reg delete "HKLM\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" ^
		/v "Elasticsearch ODBC" /f /reg:!BARCH!
	reg delete "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /f ^
		/reg:!BARCH!

	goto:eof

endlocal
