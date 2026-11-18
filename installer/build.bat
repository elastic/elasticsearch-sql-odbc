@echo off
REM Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
REM or more contributor license agreements. Licensed under the Elastic License;
REM you may not use this file except in compliance with the Elastic License.

cd %~dp0

IF NOT [%1] == [clean] (
	.paket\paket.bootstrapper.exe || GOTO :paket_failed
	IF NOT EXIST .paket\paket.exe (
		ECHO build.bat: .paket\paket.exe was not produced by the bootstrapper 1>&2
		EXIT /B 1
	)

	IF EXIST paket.lock (
		.paket\paket.exe restore || GOTO :paket_failed
	) ELSE (
		.paket\paket.exe install || GOTO :paket_failed
	)
)

SET FAKE_NO_LEGACY_WARNING=true
"packages\build\FAKE.x64\tools\FAKE.exe" "build\\scripts\\Targets.fsx" "cmdline=%*"
EXIT /B %ERRORLEVEL%

:paket_failed
EXIT /B %ERRORLEVEL%
