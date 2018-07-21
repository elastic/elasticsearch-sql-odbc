@echo off
rem
rem Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
rem or more contributor license agreements. Licensed under the Elastic License;
rem you may not use this file except in compliance with the Elastic License.
rem
REM This is just a helper script for building the ODBC driver in development.

setlocal EnableExtensions EnableDelayedExpansion
cls

set DRIVER_BASE_NAME=elasticodbc

set ARG="%*"
set SRC_PATH=%~dp0

REM "funny" fact: removing 'REM X' from above label definition makes 'cmd'
REM no longer find the label -- why? (check with "> build nobuild")
call:SET_ARCH
call:SET_TEMP
call:SET_CMAKE


REM
REM  List of variables that can be customized
REM

if [%BUILD_DIR%] == [] (
	SET BUILD_DIR=%SRC_PATH%\builds
)
if not [%ESODBC_LOG_DIR%] == [] (
	REM Strip the log level, if any present (format: path?level)
	for /f "tokens=1 delims=?" %%a in ("%ESODBC_LOG_DIR%") do set LOGGING_DIR=%%a
)
if [%INSTALL_DIR%] == [] (
	set INSTALL_DIR=%TEMP%
)


REM
REM  Perform the building steps
REM

REM presence of 'help'/'?': invoke USAGE "function" and exit
if /i not [%ARG:help=%] == [%ARG%] (
	call:USAGE %0
	goto end
) else if not [%ARG:?=%] == [%ARG%] (
	call:USAGE %0
	goto end
)

REM presence of 'proper' or 'clean': invoke respective "functions"
if /i not [%ARG:proper=%] == [%ARG%] (
	call:PROPER
	goto end
) else if /i not [%ARG:clean=%] == [%ARG%] (
	call:CLEAN
)

REM presence of 'setup': invoke SETUP "function"
if /i not [%ARG:setup=%] == [%ARG%] (
	call:SETUP
) else (
	REM Invoked without 'setup': setting up build vars skipped.

	where cl.exe >nul 2>&1
	if ERRORLEVEL 1 (
		echo.
		echo ERROR: building environment not set. Run with /? to see options.
		echo.
		goto end
	)
)


REM presence of 'fetch': invoke FETCH "function"
if /i not [%ARG:fetch=%] == [%ARG%] (
	call:FETCH
)


cd %BUILD_DIR%

REM Presence of 'clearlogs': invoke CLEARLOGS "function"
REM This needs to stay before building happens (else test logs will be
REM whipped)
if /i not [%ARG:clearlogs=%] == [%ARG%] (
	call:CLEARLOGS
) else (
	REM Invoked without 'clearlogs', logs not touched.
)

REM presence of type: invoke BUILDTYPE "function"
if /i not [%ARG:type=%] == [%ARG%] (
	call:BUILDTYPE
) else (
	echo Invoked without 'type', default applied (see usage^).
	set MSBUILD_ARGS=/p:Configuration=Debug
	set CFG_INTDIR=Debug
)

REM absence of nobuild: invoke BUILD "function";
REM 'all' and 'test' arguments presence checked inside the "function".
if /i [%ARG:nobuild=%] == [%ARG%] (
	call:BUILD
) else (
	echo Invoked with 'nobuild', building skipped.
)

REM presence of 'test': invoke TESTS "function"
if /i not [%ARG:tests=%] == [%ARG%] (
	call:TESTS
) else (
	REM Invoked without 'test': tests running skipped.
)

REM presence of 'copy': invoke COPY "function"
if /i not [%ARG:copy=%] == [%ARG%] (
	call:COPY
) else (
	REM Invoked without 'copy': DLLs test installation skipped.
)

REM presence of 'regadd': call REGADD "function"
if /i not [%ARG:regadd=%] == [%ARG%] (
	call:REGADD
) else (
	REM Invoked without 'regadd': registry adding skipped.
)

REM presence of 'regdel': invoke REGDEL "function"
if /i not [%ARG:regdel=%] == [%ARG%] (
	call:REGDEL
) else (
	REM Invoked without 'regadd': registry adding skipped.
)

:end
exit /b 0



REM
REM  "Functions"
REM

REM SET_ARCH function: set/detect the build architecture:
REM Bits and Target ARCHhitecture
:SET_ARCH
	REM presence of '32' or '64' arguments?
	if not [%ARG:32=%] == [%ARG%] (
		set TARCH=x86
		set BARCH=32
	) else if not [%ARG:64=%] == [%ARG%] (
		set TARCH=x64
		set BARCH=64

	REM is the MSVC environment already set up?
	) else if /i [%VSCMD_ARG_TGT_ARCH%] == [x86] (
		set TARCH=x86
		set BARCH=32
	) else if /i [%VSCMD_ARG_TGT_ARCH%] == [x64] (
		set TARCH=x64
		set BARCH=64


	REM neither arguments, nor MSVC environment: read OS architecture
	) else (
		reg query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && (
			set TARCH=x86
			set BARCH=32
		) || (
			set TARCH=x64
			set BARCH=64
		)
	)

	goto:eof


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

REM function to check and set cmake binary (if installed)
:SET_CMAKE
	where cmake.exe >nul 2>&1
	if ERRORLEVEL 1 (
		if exist C:\Progra~1\CMake\bin\cmake.exe (
			REM set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
			REM I don't know how to make the for-loop in :COPY work with the
			REM long format...
			set CMAKE=C:\Progra~1\CMake\bin\cmake.exe
		) else if exist "%CMAKE%" (
			REM Using already set environment path
		) else (
			echo.
			echo ERROR: needed cmake executable not found: when installed,
			echo        either set it in path or in environment variable CMAKE
			echo.
			goto end
		)
	) else (
		set CMAKE=cmake.exe
	)
	echo Using CMAKE binary: %CMAKE%

	goto:eof

REM USAGE function: output a usage message
:USAGE
	echo Usage: %1 [argument(s^)]
	echo.
	echo The following arguments are supported:
	echo    help^|*?*  : output this message and exit.
	echo    32^|64     : set the architecture to x86 or x64, respectively;
	echo                 if none is specified, autodetection is attempted.
	echo    setup     : invoke MSVC's build environment setup script before
	echo                building (requires 2017 version or later^).
	echo    clean     : remove all the files in the build dir.
	echo    proper    : clean both the libs and builds dirs and exit.
	echo    nobuild   : skip project building (the default is to build^).
	echo    type=T    : selects the build type; T can be one of Debug/Release/
	echo                RelWithDebInfo/MinSizeRel^); defaults to Debug.
	echo    exports   : dump the exported symbols in the DLL after - and
	echo                only if - building the driver.
	echo    all       : build all artifacts (driver and tests^).
	echo    tests     : run all the defined tests: invoke the 'all' build
	echo                before the 'tests' build!
	echo    suites    : compile and run each test individually, stopping at the
	echo                first failure; the 'all' or 'test' targets must be
	echo                built beforehand.
	echo    suite=S   : compile and run one test, S, individually.
	echo    copy      : copy the driver into the test dir (%INSTALL_DIR%^).
	echo    regadd    : register the driver into the registry;
	echo                (needs Administrator privileges^).
	echo    regdel    : deregister the driver from the registry;
	echo                (needs Administrator privileges^).
	echo    clearlogs : clear the logs (in: %LOGGING_DIR%^).
	echo.
	echo Multiple arguments can be used concurrently.
	echo Invoked with no arguments, the script will only initiate a build.
	echo Example:^> %1 setup 32 tests
	echo.
	echo List of read environment variables:
	echo    BUILD_DIR          : folder path to build the driver into;
	echo                         currently set to: `%BUILD_DIR%`.
	echo    ESODBC_LOG_DIR     : folder path to write logging files into;
	echo                         currently set to: `%ESODBC_LOG_DIR%`.
	echo    INSTALL_DIR        : folder path to install the driver into;
	echo                         currently set to: `%INSTALL_DIR%`.
	echo    CMAKE              : cmake executable to use (if not in path^);
	echo                         currently set to: `%CMAKE%`.
	echo.

	goto:eof


REM PROPER function: clean up the build and libs dir.
:PROPER
	echo Cleaning libs.
	if exist %BUILD_DIR%\curlclean.vcxproj (
		MSBuild %BUILD_DIR%\curlclean.vcxproj
	)
	call:CLEAN

	goto:eof


REM CLEAN function: clean up the build dir.
:CLEAN
	echo Cleaning builds.
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
	if [%EDITION%] == [] (
		echo.
		echo WARNING: no MSVC edition found, environment not set.
		echo.
	)

	goto:eof


REM BUILDTYPE function: set the build config to feed MSBuild
:BUILDTYPE
	REM cycle through the args, look for 'type' token and use the follow-up 1
	set prev=
	for %%a in (%ARG:"=%) do (
		if /i [!prev!] == [type] (
			set MSBUILD_ARGS=/p:Configuration=%%a
			set CFG_INTDIR=%%a
			echo Setting the build type to: !MSBUILD_ARGS!
			goto:eof
		) else (
			set prev=%%a
		)
	)
	set MSBUILD_ARGS=

	goto:eof

REM BUILD function: build various targets
:BUILD
	if not exist ALL_BUILD.vcxproj (
		echo Generating the project files.
		set CMAKE_ARGS=-DDRIVER_BASE_NAME=%DRIVER_BASE_NAME%
		REM no explicit x86 generator and is the default (MSVC2017 only?).
		set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_GENERATOR_PLATFORM=%TARCH:x86=%
		%CMAKE% !CMAKE_ARGS! ..
	)

	if /i not [%ARG:tests=%] == [%ARG%] (
		echo Building all the project (including tests^).
		MSBuild ALL_BUILD.vcxproj %MSBUILD_ARGS%
	) else if /i not [%ARG:all=%] == [%ARG%] (
		echo Building all the project.
		MSBuild ALL_BUILD.vcxproj %MSBUILD_ARGS%
	) else if /i not [%ARG:suites=%] == [%ARG%] (
		echo Building the test projects only.
		for %%i in (test\test_*.vcxproj) do (
			MSBuild %%~fi %MSBUILD_ARGS%
			if not ERRORLEVEL 1 (
				echo Running test\%CFG_INTDIR%\%%~ni.exe :
				test\%CFG_INTDIR%\%%~ni.exe
				if ERRORLEVEL 1 (
					goto:eof
				)
			) else (
				echo Building %%~fi failed.
				goto:eof
			)
		)
	) else if /i not [%ARG:suite==%] == [%ARG%] (
		REM cycle through the args, look for 'suite' token and use the
		REM follow-up item
		set prev=
		for %%a in (%ARG:"=%) do (
			if /i [!prev!] == [suite] (
				set SUITE=%%a
				echo Building and running one suite: !SUITE!
				echo MSBuild test\%%a.vcxproj %MSBUILD_ARGS%
				MSBuild test\%%a.vcxproj %MSBUILD_ARGS%
				if not ERRORLEVEL 1 (
					test\%CFG_INTDIR%\%%a.exe
				)
				goto:eof
			) else (
				set prev=%%a
			)
		)
	) else (
		echo Building the driver only.
		REM file name expansion, cmd style...
		for /f %%i in ("%DRIVER_BASE_NAME%*.vcxproj") do MSBuild %%~nxi %MSBUILD_ARGS%

		if not ERRORLEVEL 1 if /i not [%ARG:symbols=%] == [%ARG%] (
			dumpbin /exports %CFG_INTDIR%\%DRIVER_BASE_NAME%*.dll
		)
	)

	goto:eof

REM TESTS function: run the compiled tests
:TESTS
	REM if called with nobuild, but test, this will trigger the build
	if not exist RUN_TESTS.vcxproj (
		call:BUILD
	)
	MSBuild RUN_TESTS.vcxproj !MSBUILD_ARGS!

	goto:eof

REM COPY function: copy DLLs (libcurl, odbc) to the test "install" dir
:COPY
	echo Copying into test install folder %INSTALL_DIR%.
	copy %CFG_INTDIR%\%DRIVER_BASE_NAME%*.dll %INSTALL_DIR%

	REM Read LIBCURL_LD_PATH value from cmake's cache
	for /f "tokens=2 delims==" %%i in ('%CMAKE% -L %BUILD_DIR% 2^>NUL ^| find "LIBCURL_LD_PATH"') do set LIBCURL_LD_PATH=%%i
	REM change the slashes' direction
	set LIBCURL_LD_PATH=%LIBCURL_LD_PATH:/=\%
	copy %LIBCURL_LD_PATH%\libcurl.dll  %INSTALL_DIR%

	goto:eof


REM CLEARLOGS function: empty logs files
:CLEARLOGS
	if not [%LOGGING_DIR%] == [] (
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
	if not exist %BUILD_DIR%\%CFG_INTDIR%\%DRIVER_BASE_NAME%*.dll (
		echo Error: Driver can only be added into the registry once built.
		goto end
	)
	for /f %%i in ("%BUILD_DIR%\%CFG_INTDIR%\%DRIVER_BASE_NAME%*.dll") do set DRVNAME=%%~nxi

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
