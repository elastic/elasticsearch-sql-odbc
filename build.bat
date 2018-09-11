@echo off
rem
rem Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
rem or more contributor license agreements. Licensed under the Elastic License;
rem you may not use this file except in compliance with the Elastic License.
rem
REM This is just a helper script for building the ODBC driver in development.

setlocal EnableExtensions EnableDelayedExpansion
cls

set DRIVER_BASE_NAME=esodbc

set ARG="%*"
set SRC_PATH=%~dp0

REM "funny" fact: removing 'REM X' from above label definition makes 'cmd'
REM no longer find the label -- why? (check with "> build nobuild")
call:SET_ARCH
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

REM
REM  Perform the building steps
REM

REM presence of 'help'/'?': invoke USAGE "function" and exit
if /i not [%ARG:help=%] == [%ARG%] (
	call:USAGE %0
	goto END
) else if not [%ARG:?=%] == [%ARG%] (
	call:USAGE %0
	goto END
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
		goto END
	)
)

REM presence of 'proper' or 'clean': invoke respective "functions"
if /i not [%ARG:proper=%] == [%ARG%] (
	call:PROPER
	goto END
) else if /i not [%ARG:clean=%] == [%ARG%] (
	call:CLEAN
)


REM
REM Run building steps from within build dir
REM

cd %BUILD_DIR%


REM presence of 'install'/'package': invoke respective _CONF "function"
if /i not [%ARG:install=%] == [%ARG%] (
	if /i not [%ARG:package=%] == [%ARG%] (
		REM Since CPACK uses the install target:
		echo ERROR: 'install' and 'package' are mutually exclusive actions.
		exit /b 1
	)
	call:INSTALL_CONF
) else if /i not [%ARG:package=%] == [%ARG%] (
	call:PACKAGE_CONF
)

REM presence of type: invoke BUILDTYPE "function"
if /i not [%ARG:type=%] == [%ARG%] (
	call:BUILDTYPE
) else (
	set BUILD_TYPE=Debug
	set MSBUILD_ARGS=/p:Configuration=!BUILD_TYPE!
	echo Invoked without 'type', !BUILD_TYPE!-building (default^).
)

REM absence of nobuild: invoke BUILD "function";
REM 'all' and 'test' arguments presence checked inside the "function".
if /i [%ARG:nobuild=%] == [%ARG%] (
	REM build libraries/tests
	call:BUILD
	if ERRORLEVEL 1 (
		goto END
	)

	REM run test(s)
	call:TESTS_SUITE_S
	if ERRORLEVEL 1 (
		goto END
	)
) else (
	echo Invoked with 'nobuild', building skipped.
)


REM presence of 'install'/'package': invoke respective _DO "function"
if /i not [%ARG:install=%] == [%ARG%] (
	call:INSTALL_DO
) else if /i not [%ARG:package=%] == [%ARG%] (
	call:PACKAGE_DO
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


REM function to check and set cmake binary (if installed)
:SET_CMAKE
	where cmake.exe >nul 2>&1
	if ERRORLEVEL 1 (
		if exist C:\Progra~1\CMake\bin\cmake.exe (
			REM set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
			set CMAKE=C:\Progra~1\CMake\bin\cmake.exe
		) else if exist "%CMAKE%" (
			REM Using already set environment path
		) else (
			echo.
			echo ERROR: needed cmake executable not found: when installed,
			echo        either set it in PATH or in environment variable CMAKE
			echo.
			goto END
		)
	) else (
		set CMAKE=cmake.exe
	)
	echo Using CMAKE binary: %CMAKE%

	goto:eof

REM USAGE function: output a usage message
:USAGE
	echo Usage: %~1 [argument(s^)]
	echo.
	echo The following arguments are supported:
	echo    help^|*?*    : output this message and exit; ?? for more options.
	echo    32^|64       : set the architecture to x86 or x64, respectively;
	echo                   if none is specified, autodetection is attempted.
	echo    setup       : invoke MSVC's build environment setup script before
	echo                  building (requires 2017 version or later^).
	echo    clean       : remove all the files in the build dir.
	echo    proper      : clean libs, builds, project dirs and exit.
	echo    type:T      : selects the build type, T, among one of:
	echo                  Debug/Release/RelWithDebInfo/MinSizeRel.
	echo    tests       : run all the defined tests.
	echo    suites      : run all the defined tests, individually.
	echo    suite:S     : run one test, S.
	echo    install[:D] : install the driver files. D, the target directory
	echo                  path can only be specified before the project/make
	echo                  files are generated.
	echo    package[:V] : package the driver files. V is a versioning string
	echo                  that will be added to the package file name and can
	echo                  can only be specified before the project/make files
	echo                  are generated.
	echo.
	echo Installing and packaging are mutually exclusive.
	echo Multiple arguments can be used concurrently. Examples:
	echo - set up a x86 build environment and running the tests:
	echo   ^> %~1 setup 32 tests
	echo - remove any existing build and create a release archieve:
	echo   ^> %~1 clean package:-final type:Release
	echo.
	echo List of read environment variables:
	echo    CMAKE       : cmake executable to use (if not in PATH^);
	echo                  currently set to: `%CMAKE%`.
	echo    BUILD_DIR   : folder path to build the driver into;
	echo                  currently set to: `%BUILD_DIR%`.
	echo    INSTALL_DIR : folder path to install the driver into;
	echo                  currently set to: `%INSTALL_DIR%`.
	echo.

	if [%ARG:??=%] == [%ARG%] (
		goto:eof
	)
	echo Extra development arguments:
	echo    nobuild   : skip project building (the default is to build^).
	echo    genonly   : generate project/make files, but don't build anything.
	echo    exports   : dump the exported symbols in the DLL after buildint it.
	echo    regadd    : register the driver into the registry;
	echo                (needs Administrator privileges^).
	echo    regdel    : deregister the driver from the registry;
	echo                (needs Administrator privileges^).
	echo.
	goto:eof


REM PROPER function: clean up the build and libs dir.
:PROPER
	echo Cleaning libs.
	if exist %BUILD_DIR%\curlclean.vcxproj (
		MSBuild %BUILD_DIR%\curlclean.vcxproj
	)
	call:CLEAN
	REM delete VisualStudio files
	if exist .vs (
		rmdir /S /Q .vs
	)

	goto:eof


REM CLEAN function: clean up the build dir.
:CLEAN
	echo Cleaning builds.
	REM delete all files that don't have the "extension" .gitignore :-)
	for %%i in (%BUILD_DIR%\*) do if not %%~xi == .gitignore (
		del /s /q %%i >nul 2>&1
	)
	REM delete all directories
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

REM INSTALL_CONF function: extract install location, if any; this will be
REM injected into the project files generated by cmake
:INSTALL_CONF
	if exist ALL_BUILD.vcxproj (
		echo Project files already generated, install configuration skipped.
		goto:eof
	)
	if not [%INSTALL_DIR%] == [] (
		echo Installation directory set by INSTALL_DIR=%INSTALL_DIR%
		goto:eof
	)

	REM cycle through the args, look for 'install:' token and use the
	REM follow-up token
	for %%a in (%ARG:"=%) do (
		set crr=%%a
		if /i ["!crr:~0,8!"] == ["install:"] (
			set INSTALL_DIR=!crr:~8!
			echo Setting the installation dir to: !INSTALL_DIR!
			goto:eof
		)
	)
	set INSTALL_DIR=

	goto:eof

REM PACKAGE_CONF function: extract versioning string, if any; this will be
REM injected into the project files generated by cmake
:PACKAGE_CONF
	if exist ALL_BUILD.vcxproj (
		echo Project files already generated, package configuration skipped.
		goto:eof
	)

	REM cycle through the args, look for 'package:' token and use the
	REM follow-up token
	for %%a in (%ARG:"=%) do (
		set crr=%%a
		if /i ["!crr:~0,8!"] == ["package:"] (
			set PACKAGE_VER=!crr:~8!
			echo Setting the packaging version string to: !PACKAGE_VER!

			REM CPACK needs a relative prefix in order to install the files
			REM correctly.
			set INSTALL_DIR=.
			goto:eof
		)
	)
	set PACKAGE_VER=

	goto:eof

REM BUILDTYPE function: set the build config to feed MSBuild
:BUILDTYPE
	REM cycle through the args, look for 'type:' token and use the
	REM follow-up token
	for %%a in (%ARG:"=%) do (
		set crr=%%a
		if /i ["!crr:~0,5!"] == ["type:"] (
			set BUILD_TYPE=!crr:~5!
			set MSBUILD_ARGS=/p:Configuration=!BUILD_TYPE!
			echo Setting the build type to: !MSBUILD_ARGS!
			goto:eof
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
		set CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_GENERATOR_PLATFORM=%TARCH:x86=%
		if not [!INSTALL_DIR!] == [] (
			set CMAKE_ARGS=!CMAKE_ARGS! -DINSTALL_DIR=!INSTALL_DIR!
		)
		if not [!PACKAGE_VER!] == [] (
			set CMAKE_ARGS=!CMAKE_ARGS! -DVERSION_QUALIFIER=!PACKAGE_VER!
		)

		echo cmake params: !CMAKE_ARGS!
		%CMAKE% !CMAKE_ARGS! !SRC_PATH!
	)
	if /i not [%ARG:genonly=%] == [%ARG%] (
		goto:eof
	)

	if /i not [%ARG:tests=%] == [%ARG%] (
		echo Building all the project.
		MSBuild ALL_BUILD.vcxproj %MSBUILD_ARGS%
		if ERRORLEVEL 1 (
			goto END
		)
	) else (
		echo Building the driver.
		REM file name expansion, cmd style...
		for /f %%i in ("%DRIVER_BASE_NAME%*.vcxproj") do (
			MSBuild %%~nxi %MSBUILD_ARGS%
			if ERRORLEVEL 1 (
				goto END
			)
			if /i not [%ARG:exports=%] == [%ARG%] (
				dumpbin /exports %BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll
			)
		)

		if /i not [%ARG:suiteS=%] == [%ARG%] (
			echo Building the test projects.
			for %%i in (test\test_*.vcxproj) do (
				MSBuild %%~fi %MSBUILD_ARGS%
				if ERRORLEVEL 1 (
					goto END
				)
			)
		) else if /i not [%ARG:suite==%] == [%ARG%] (
			REM cycle through the args, look for 'suite:' token and use the
			REM follow-up token
			for %%a in (%ARG:"=%) do (
				set crr=%%a
				if /i ["!crr:~0,6!"] == ["suite:"] (
					set SUITE=!crr:~6!
					echo Building one suite: !SUITE!
					MSBuild test\!SUITE!.vcxproj %MSBUILD_ARGS%
					if ERRORLEVEL 1 (
						goto END
					)
					goto:eof
				)
			)
		)
	)

	goto:eof

REM TESTS_SUITE_S function: run the compiled tests
:TESTS_SUITE_S
	if /i not [%ARG:tests=%] == [%ARG%] (
		MSBuild RUN_TESTS.vcxproj !MSBUILD_ARGS!
		if ERRORLEVEL 1 (
			goto END
		)
	) else if /i not [%ARG:suiteS=%] == [%ARG%] (
		echo Running all test suites.
		for %%i in (test\test_*.vcxproj) do (
			echo Running test\%BUILD_TYPE%\%%~ni.exe :
			test\%BUILD_TYPE%\%%~ni.exe
			if ERRORLEVEL 1 (
				goto END
			)
		)
	) else if /i not [%ARG:suite==%] == [%ARG%] (
		REM cycle through the args, look for 'suite:' token and use the
		REM follow-up item
		for %%a in (%ARG:"=%) do (
			set crr=%%a
			if /i ["!crr:~0,6!"] == ["suite:"] (
				set SUITE=!crr:~6!
				echo Running one suite: !SUITE!
				test\%BUILD_TYPE%\!SUITE!.exe
				if ERRORLEVEL 1 (
					goto END
				)
				goto:eof
			)
		)
	)

	goto:eof

REM INSTALL_DO function: copy DLLs (libcurl, odbc) into the install
:INSTALL_DO
	echo Installing the driver files.
	MSBuild INSTALL.vcxproj !MSBUILD_ARGS!
	if ERRORLEVEL 1 (
		goto END
	)

	goto:eof

REM PACKAGE_DO function: generate deliverable package
:PACKAGE_DO
	echo Packaging the driver files.
	MSBuild PACKAGE.vcxproj !MSBUILD_ARGS!

	goto:eof

REM COPY function: copy DLLs (libcurl, odbc) to the test "install" dir
:COPY
	echo Copying into test install folder %INSTALL_DIR%.
	copy %BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll %INSTALL_DIR%

	REM Read LIBCURL_LD_PATH value from cmake's cache
	for /f "tokens=2 delims==" %%i in ('%CMAKE% -L %BUILD_DIR% 2^>NUL ^| find "LIBCURL_LD_PATH"') do set LIBCURL_LD_PATH=%%i
	REM change the slashes' direction
	set LIBCURL_LD_PATH=%LIBCURL_LD_PATH:/=\%
	copy %LIBCURL_LD_PATH%\libcurl.dll  %INSTALL_DIR%

	goto:eof


REM REGADD function: add driver into the registry
:REGADD
	echo Adding driver into the registry.

	REM check if driver exists, otherwise the filename is unknown
	if not exist %BUILD_DIR%\%BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll (
		echo Error: Driver can only be added into the registry once built.
		goto END
	)
	for /f %%i in ("%BUILD_DIR%\%BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll") do set DRVNAME=%%~nxi

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



:END
	exit /b %ERRORLEVEL%

endlocal
