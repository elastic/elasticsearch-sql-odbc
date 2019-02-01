@echo off
rem
rem Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
rem or more contributor license agreements. Licensed under the Elastic License;
rem you may not use this file except in compliance with the Elastic License.
rem

setlocal EnableExtensions EnableDelayedExpansion

set DRIVER_BASE_NAME=esodbc

set ARG="%*"
set SRC_PATH=%~dp0

REM presence of 'help'/'?': invoke USAGE "function" and exit
if /i not [%ARG:help=%] == [%ARG%] (
	call:USAGE %0
	goto END
) else if not [%ARG:?=%] == [%ARG%] (
	call:USAGE %0
	goto END
)

call:SET_ARCH
call:SET_CMAKE
call:SET_PYTHON
call:SET_BUILDS_DIR


REM
REM  Perform the building steps
REM

if /i not [%ARG:ctests=%] == [%ARG%] (
	call:CTESTS
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
		echo %~nx0: ERROR: building environment not set. Run with /? to see options.
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

if not exist !BUILD_DIR! (
	mkdir !BUILD_DIR!
	echo %~nx0: building in !BUILD_DIR!.
)
cd %BUILD_DIR%


REM presence of 'install'/'package': invoke respective _CONF "function"
if /i not [%ARG:install=%] == [%ARG%] (
	if /i not [%ARG:package=%] == [%ARG%] (
		REM Since CPACK uses the install target:
		echo %~nx0: ERROR: 'install' and 'package' are mutually exclusive actions.
		exit /b 1
	)
	call:INSTALL_CONF
) else if /i not [%ARG:package=%] == [%ARG%] (
	call:PACKAGE_CONF
)

REM absence of nobuild: invoke BUILD "function";
REM 'all' and 'test' arguments presence checked inside the "function".
if /i [%ARG:nobuild=%] == [%ARG%] (
	REM build libraries/utests
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
	echo %~nx0: invoked with 'nobuild', building skipped.
)

REM presence of 'install'/'package': invoke respective _DO "function"
if /i not [%ARG:install=%] == [%ARG%] (
	call:INSTALL_DO
) else if /i not [%ARG:package=%] == [%ARG%] (
	call:PACKAGE_DO
)

REM presence of 'itests': run the integration tests
if /i not [%ARG:itests=%] == [%ARG%] (
	call:ITESTS
	if ERRORLEVEL 1 (
		goto END
	)
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
			echo %~nx0: ERROR: needed cmake executable not found: when
			echo               installed, either set it in PATH or in 
			echo               environment variable CMAKE.
			echo.
			goto END
		)
	) else (
		set CMAKE=cmake.exe
	)
	echo|set /p="%~nx0: using CMAKE binary: %CMAKE% : "
	%CMAKE% --version | findstr /C:"version"

	goto:eof

REM function to check and set the python interpreter (if installed)
:SET_PYTHON
	py -3-64 -V >nul 2>&1
	if ERRORLEVEL 1 (
		echo.
		echo %~nx0: WARNING: Python version 3 for x86_64 not found.
		echo %~nx0:          Integration tests for this platform disabled.
		echo.

		set PY3_64=
	) else (
		set PY3_64=py -3-64 -tt
	)

	py -3-32 -V >nul 2>&1
	if ERRORLEVEL 1 (
		echo.
		echo %~nx0: WARNING: Python version 3 for x86 not found.
		echo %~nx0:          Integration tests for this platform disabled.
		echo.

		set PY3_32=
	) else (
		set PY3_32=py -3-32 -tt
	)

	goto:eof

REM function to create dirs where to build and set corresponding build vars:
REM build: path to either x86 or x64 build
REM buildS: path to store the x86 and/or x64 folders (no Release/Debug
REM specific dirs, though)
:SET_BUILDS_DIR
	if [%BUILDS_DIR%] == [] (
		SET BUILDS_DIR=!SRC_PATH!\builds
	)
	if not exist !BUILDS_DIR! (
		REM Happened before: builds/.gitignore gets removed and then
		REM so does builds/ in git.
		echo.
		echo %~nx0: ERROR: !BUILDS_DIR! to hold the builds doesn't exist.
		echo.

		goto END
	)

	SET BUILD_DIR=!BUILDS_DIR!\!TARCH!
	SET INSTALLER_OUT_DIR=!SRC_PATH!\installer\build\out

	goto:eof

REM USAGE function: output a usage message
:USAGE
	echo Usage: %~1 [argument(s^)]
	echo.
	echo The following arguments are supported:
	echo    help^|*?*    : output this message and exit; ?? for more options.
	echo    32^|64       : set the architecture to x86 or x64, respectively;
	echo                  if none is specified, autodetection is attempted.
	echo    setup       : invoke MSVC's build environment setup script before
	echo                  building (requires 2017 version or later^).
	echo    clean       : remove all the files in the build dir.
	echo    proper      : clean libs, builds, project dirs and exit.
	echo    type:T      : selects the build type, T: Debug or Release.
	echo    ctests      : run the CI testing - integration and unit tests,
	echo                  x86 and x64 architectures in Release and Debug mode
	echo                  - and exits. (All the other arguments are ignored.^)
	echo    itests[:I]  : run the integration tests. If I, the path to the
	echo                  packaged driver (installer^) is not provided, this
	echo                  will be looked for in the installer output folder.
	echo    utests      : run all the defined unit tests.
	echo    suites      : run all the defined unit tests, individually.
	echo    suite:U     : run one unit test, U.
	echo    package[:V] : generate the installer. V is a versioning string
	echo                  that will be added to the installer file name and can
	echo                  can only be specified before the project/make files
	echo                  are generated.
	echo    sign[:C+P]  : sign the installer file. C is the certificate file to
	echo                  use and P the file containing the password for it (if
	echo                  any^).
	echo.
	echo Installing and packaging are mutually exclusive.
	echo Multiple arguments can be used concurrently. Examples:
	echo - set up a x86 build environment and running the unit tests:
	echo   ^> %~1 setup 32 utests
	echo - remove any existing build and create a release archieve:
	echo   ^> %~1 clean package:-final type:Release
	echo.
	echo List of read environment variables:
	echo    CMAKE       : cmake executable to use (if not in PATH^);
	echo                  currently set to: `%CMAKE%`.
	echo    BUILDS_DIR  : folder path to build the driver into;
	echo                  currently set to: `%BUILDS_DIR%`.
	echo    INSTALL_DIR : folder path to install the driver into;
	echo                  currently set to: `%INSTALL_DIR%`.
	echo.

	if [%ARG:??=%] == [%ARG%] (
		goto:eof
	)
	echo Extra development arguments:
	echo    nobuild     : skip project building (the default is to build^).
	echo    genonly     : generate project/make files, but don't build.
	echo    curldll     : link libcurl dynamically.
	echo    exports     : dump the exported symbols in the DLL after building.
	echo    depends     : dump the dependents libs of the build DLL.
	echo    install[:D] : install the driver files. D, the target directory
	echo                  path can only be specified before the project/make
	echo                  files are generated. (This does not run the
	echo                  installer.^)
	echo    regadd      : register the driver into the registry unde
	echo                  'Elasticsearch ODBC' name;
	echo                  (needs Administrator privileges^).
	echo    regdel      : deregister the driver from the registry;
	echo                  (needs Administrator privileges^).
	echo    tests       : (deprecated^) synonym with utests.
	echo.
	goto:eof

REM CTESTS function: run CI testing
:CTESTS
	echo %~nx0: Running 32-bit unit tests.
	call %SRC_PATH%\build.bat clean setup 32 utests
	if ERRORLEVEL 1 goto END
	echo %~nx0: Running 64-bit unit tests.
	call %SRC_PATH%\build.bat clean setup 64 utests
	if ERRORLEVEL 1 goto END
	echo %~nx0: Running 32-bit unit integration tests.
	call %SRC_PATH%\build.bat clean setup 32 package:-SNAPSHOT type:Release itests
	if ERRORLEVEL 1 goto END
	echo %~nx0: Running 64-bit unit integration tests.
	call %SRC_PATH%\build.bat clean setup 64 package:-SNAPSHOT type:Release itests
	if ERRORLEVEL 1 goto END

	echo %~nx0: SUCCESS!

	goto:eof

REM PROPER function: clean up the build and libs dir.
:PROPER
	echo %~nx0: cleaning libs.
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
	echo %~nx0: cleaning builds.
	REM delete all files that don't have the "extension" .gitignore :-)
	for %%i in (%BUILDS_DIR%\*) do if not %%~xi == .gitignore (
		del /s /q %%i >nul 2>&1
	)
	REM delete all directories
	for /d %%i in (%BUILDS_DIR%\*) do rmdir /s /q %%i >nul 2>&1

	REM delete Python's bytecode if any
	rmdir /s /q test\integration\__pycache__ >nul 2>&1

	REM clean the installer too
	!SRC_PATH!\installer\build.bat clean

	goto:eof


REM SETUP function: set-up the build environment
:SETUP
	set RELEASE=2017
	for %%e in (Enterprise, Professional, Community) do (
		if exist "C:\Program Files (x86)\Microsoft Visual Studio\%RELEASE%\%%e\Common7\Tools\VsDevCmd.bat" (
			if /i "%%e" == "Community" (
				echo.
				echo %~nx0: WARNING: Community edition is not licensed to build commerical projects.
				echo.
			)
			call "C:\Program Files (x86)\Microsoft Visual Studio\%RELEASE%\%%e\Common7\Tools\VsDevCmd.bat" -arch=!TARCH!
			set EDITION=%%e
			goto:eof
		)
	)
	if [%EDITION%] == [] (
		echo.
		echo %~nx0: WARNING: no MSVC edition found, environment not set.
		echo.
	)

	goto:eof

REM INSTALL_CONF function: extract install location, if any; this will be
REM injected into the project files generated by cmake
:INSTALL_CONF
	if exist ALL_BUILD.vcxproj (
		echo %~nx0: NOTICE: build files already generated, install configuration skipped.
		goto:eof
	)
	if not [%INSTALL_DIR%] == [] (
		echo %~nx0: installation directory set by INSTALL_DIR=%INSTALL_DIR%.
		goto:eof
	)

	REM cycle through the args, look for '^install' token and use the
	REM follow-up token
	for %%a in (%ARG:"=%) do (
		set crr=%%a
		if /i ["!crr:~0,7!"] == ["install"] (
			set INSTALL_DIR=!crr:~8!
			echo %~nx0: Setting the installation dir to: !INSTALL_DIR!.
			goto:eof
		)
	)
	set INSTALL_DIR=

	goto:eof

REM PACKAGE_CONF function: extract versioning string, if any; this will be
REM injected into the project files generated by cmake
:PACKAGE_CONF
	if exist ALL_BUILD.vcxproj (
		echo %~nx0: NOTICE: build files already generated, package configuration skipped.
		goto:eof
	)

	set PACKAGE_VER=
	set SIGN_PATH_CERT=
	set SIGN_PATH_PASS=
	REM cycle through the args, look for '^package' token and use the
	REM follow-up token
	for %%a in (%ARG:"=%) do (
		set crr=%%a
		if /i ["!crr:~0,7!"] == ["package"] (
			set PACKAGE_VER=!crr:~8!
			echo %~nx0: setting the packaging version string to: !PACKAGE_VER!.

			REM CPACK needs a relative prefix in order to install the files
			REM correctly.
			set INSTALL_DIR=.
		)
		if /i ["!crr:~0,5!"] == ["sign:"] (
			set sign=!crr:~5!
			echo %~nx0: signing with: !sign!
			for /f "tokens=1,2 delims=+ " %%a in ("!sign!") do (
				set SIGN_PATH_CERT=%%a
				set SIGN_PATH_PASS=%%b
			)
			echo %~nx0: signing paths for certificate: !SIGN_PATH_CERT!, password: !SIGN_PATH_PASS!
		)
	)

	goto:eof

REM BUILDTYPE function: set the build config to feed MSBuild
:BUILDTYPE
	if not exist ALL_BUILD.vcxproj (
		if /i not [%ARG:type=%] == [%ARG%] (
			REM cycle through the args, look for 'type:' token and use the
			REM follow-up token
			for %%a in (%ARG:"=%) do (
				set crr=%%a
				if /i ["!crr:~0,5!"] == ["type:"] (
					set BUILD_TYPE=!crr:~5!
				)
			)
			REM no check against empty val (type:) here
		)
		if [!BUILD_TYPE!] == [] (
			set BUILD_TYPE=Debug
		)
		echo %~nx0: setting the build type to: !BUILD_TYPE!.
	) else if exist %BUILD_DIR%/Release (
			set BUILD_TYPE=Release
			echo %~nx0: previously build type set: !BUILD_TYPE!.
	) else if exist %BUILD_DIR%/Debug (
			set BUILD_TYPE=Debug
			echo %~nx0: previously build type set: !BUILD_TYPE!.
	) else (
		REM DSN editor libs only support Debug and Release
		echo %~nx0: ERROR: unknown previously set build type.
		set ERRORLEVEL=1
		goto END
	)
	set MSBUILD_ARGS=/p:Configuration=!BUILD_TYPE!

	goto:eof

REM BUILD function: build various targets
:BUILD
	REM set the wanted or previously set build type.
	call:BUILDTYPE
	if ERRORLEVEL 1 (
		goto END
	)
	if not exist ALL_BUILD.vcxproj (
		echo %~nx0: generating the project files.

		REM set the wanted build type.
		rem call:BUILDTYPE

		set CMAKE_ARGS=-DDRIVER_BASE_NAME=%DRIVER_BASE_NAME%
		REM no explicit x86 generator and is the default (MSVC2017 only?).
		set CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_GENERATOR_PLATFORM=%TARCH:x86=%

		if /i not [%ARG:curldll=%] == [%ARG%] (
			set CMAKE_ARGS=!CMAKE_ARGS! -DLIBCURL_LINK_MODE=dll
		)
		if /i [!BUILD_TYPE!] == [Debug] (
			set CMAKE_ARGS=!CMAKE_ARGS! -DLIBCURL_BUILD_TYPE=debug
		) else (
			set CMAKE_ARGS=!CMAKE_ARGS! -DLIBCURL_BUILD_TYPE=release
		)

		if not [!INSTALL_DIR!] == [] (
			set CMAKE_ARGS=!CMAKE_ARGS! -DINSTALL_DIR=!INSTALL_DIR!
		)
		if not [!PACKAGE_VER!] == [] (
			set CMAKE_ARGS=!CMAKE_ARGS! -DVERSION_QUALIFIER=!PACKAGE_VER!
		)

		echo %~nx0: cmake params: !CMAKE_ARGS!.
		%CMAKE% !CMAKE_ARGS! !SRC_PATH!
	)
	if /i not [%ARG:genonly=%] == [%ARG%] (
		goto:eof
	)

	if /i not [%ARG: tests=%] == [%ARG%] ( REM utests dup'd
		echo %~nx0: building all the project.
		MSBuild ALL_BUILD.vcxproj %MSBUILD_ARGS%
		if ERRORLEVEL 1 (
			goto END
		)
	) else if /i not [%ARG:utests=%] == [%ARG%] (
		echo %~nx0: building all the project.
		MSBuild ALL_BUILD.vcxproj %MSBUILD_ARGS%
		if ERRORLEVEL 1 (
			goto END
		)
	) else (
		echo %~nx0: building the driver.
		REM file name expansion, cmd style...
		for /f %%i in ("%DRIVER_BASE_NAME%*.vcxproj") do (
			MSBuild %%~nxi %MSBUILD_ARGS%
			if ERRORLEVEL 1 (
				goto END
			)
			if /i not [%ARG:exports=%] == [%ARG%] (
				dumpbin /exports %BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll
			)
			if /i not [%ARG:depends=%] == [%ARG%] (
				dumpbin /dependents %BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll
			)
		)

		if /i not [%ARG:suiteS=%] == [%ARG%] (
			echo %~nx0: building the test projects.
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
					echo %~nx0: building one suite: !SUITE!.
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

REM TESTS_SUITE_S function: run the compiled unit tests
:TESTS_SUITE_S
	if /i not [%ARG: tests=%] == [%ARG%] ( REM utests dup'd
		MSBuild RUN_TESTS.vcxproj !MSBUILD_ARGS!
		if ERRORLEVEL 1 (
			goto END
		)
	) else if /i not [%ARG:utests=%] == [%ARG%] (
		MSBuild RUN_TESTS.vcxproj !MSBUILD_ARGS!
		if ERRORLEVEL 1 (
			goto END
		)
	) else if /i not [%ARG:suiteS=%] == [%ARG%] (
		echo %~nx0: running all test suites.
		for %%i in (test\test_*.vcxproj) do (
			echo %~nx0: running test\%BUILD_TYPE%\%%~ni.exe :
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
				echo %~nx0: running one suite: !SUITE!.
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
	echo %~nx0: installing the driver files.
	MSBuild INSTALL.vcxproj !MSBUILD_ARGS!
	if ERRORLEVEL 1 (
		goto END
	)

	goto:eof

REM PACKAGE_DO function: generate deliverable package
:PACKAGE_DO
	echo %~nx0: packaging the driver files.
	MSBuild PACKAGE.vcxproj !MSBUILD_ARGS!
	if ERRORLEVEL 1 (
		goto END
	)
	for /f %%i in ("%BUILD_DIR%\%DRIVER_BASE_NAME%*.zip") do (
		if not [!SIGN_PATH_CERT!] == [] (
			echo %~nx0: building release installer for ZIP: %%~fi.
			!SRC_PATH!\installer\build.bat release %%~fi !SIGN_PATH_CERT! !SIGN_PATH_PASS!
		) else (
			echo %~nx0: building installer for ZIP: %%~fi.
			!SRC_PATH!\installer\build.bat buildinstaller %%~fi
		)
		if ERRORLEVEL 1 (
			goto END
		)
	)

	goto:eof

:ITESTS
	echo %~nx0: Running integration tests.
	set INSTALLER_PATH=
	REM cycle through the args, look for '^itests' token and use the follow-up
	for %%a in (%ARG:"=%) do (
		set crr=%%a
		if /i ["!crr:~0,6!"] == ["itests"] (
			set INSTALLER_PATH=!crr:~7!
		)
	)
	if [!INSTALLER_PATH!] == [] (
		set X64_INSTALLER=
		set X32_INSTALLER=
		for %%f in (!INSTALLER_OUT_DIR!\*x86_64.msi) do (
			if not [!X64_INSTALLER!] == [] (
				echo %~nx0: WARNING: multiple x64 installers found.
			)
			set X64_INSTALLER=%%f
		)
		for %%f in (!INSTALLER_OUT_DIR!\*x86.msi) do (
			if not [!X32_INSTALLER!] == [] (
				echo %~nx0: WARNING: multiple x86 installers found.
			)
			set X32_INSTALLER=%%f
		)
	) else (
		echo !INSTALLER_PATH! | findstr "x86_64.msi\>" >nul 2>&1
		if ERRORLEVEL 0 (
			set X64_INSTALLER=!INSTALLER_PATH!
		) else (
			echo !INSTALLER_PATH! | findstr "x86.msi\>" >nul 2>&1
			if ERRORLEVEL 0 (
				set X32_INSTALLER=!INSTALLER_PATH!
			)
		)
	)

	if [!X64_INSTALLER!] == [] if [!X32_INSTALLER!] == [] (
		echo %~nx0: ERROR: no installer to test with available.
		set ERRORLEVEL=1
		goto END
	)

	if not [!X64_INSTALLER!] == [] (
		if [!PY3_64!] == [] (
			echo %~nx0: ERROR: no Python 3 x86_64 available.
			set ERRORLEVEL=1
			goto END
		)
		echo %~nx0: Integration testing with: !X64_INSTALLER!.
		!PY3_64! !SRC_PATH!\test\integration\ites.py -r !BUILD_DIR! -d !X64_INSTALLER!
		if ERRORLEVEL 1 (
			goto END
		)
	)
	if not [!X32_INSTALLER!] == [] (
		if [!PY3_32!] == [] (
			echo %~nx0: ERROR: no Python 3 x86 available.
			set ERRORLEVEL=1
			goto END
		)
		echo %~nx0: Integration testing with: !X32_INSTALLER!.
		!PY3_32! !SRC_PATH!\test\integration\ites.py -r !BUILD_DIR! -d !X32_INSTALLER!
		if ERRORLEVEL 1 (
			goto END
		)
	)

	goto:eof

REM REGADD function: add driver into the registry
:REGADD
	echo %~nx0: adding driver into the registry.

	REM check if driver exists, otherwise the filename is unknown
	if not exist %BUILD_DIR%\%BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll (
		echo %~nx0: ERROR: driver can only be added into the registry once built.
		goto END
	)
	for /f %%i in ("%BUILD_DIR%\%BUILD_TYPE%\%DRIVER_BASE_NAME%*.dll") do set DRVNAME=%%~nxi

	echo %~nx0: adding ESODBC driver %INSTALL_DIR%\!DRVNAME! to the registry.

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
	echo %~nx0: removing ESODBC driver from the registry.

	reg delete "HKLM\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" ^
		/v "Elasticsearch ODBC" /f /reg:!BARCH!
	reg delete "HKLM\SOFTWARE\ODBC\ODBCINST.INI\Elasticsearch ODBC" /f ^
		/reg:!BARCH!

	goto:eof



:END
	exit /b %ERRORLEVEL%

endlocal
