REM Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
REM or more contributor license agreements. Licensed under the Elastic License;
REM you may not use this file except in compliance with the Elastic License.

@echo off

.paket\paket.bootstrapper.exe
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

IF EXIST paket.lock (
	.paket\paket.exe restore
	IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%
) ELSE (
	.paket\paket.exe install
	IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%
)

SET FAKE_NO_LEGACY_WARNING=true
"packages\build\FAKE.x64\tools\FAKE.exe" "build\\scripts\\Targets.fsx" "cmdline=%*"
