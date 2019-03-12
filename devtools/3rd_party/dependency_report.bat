@echo off
rem
rem Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
rem or more contributor license agreements. Licensed under the Elastic License;
rem you may not use this file except in compliance with the Elastic License.
rem

rem
rem Create a CSV file listing the information about our 3rd party dependencies
rem that is required for the stack-wide list.
rem
rem Usage:
rem dependency_report.bat --csv <output_file>
rem
rem The format is that defined in https://github.com/elastic/release-manager/issues/207,
rem i.e. a CSV file with the following fields:
rem
rem name,version,revision,url,license,copyright
rem
rem The way this script works, each component must have its own CSV file with
rem those fields, and this script simply combines them into a single CSV file.
rem Because of this, the field order is important - in each per-component CSV
rem file the fields must be in the order shown above.

setlocal EnableExtensions EnableDelayedExpansion

if /i [%1] == [--csv] (
	set OUTPUT_FILE=%2
)

if [%OUTPUT_FILE%] == [] (
    echo Usage: %~0 --csv ^<output_file^>
	exit /b 1
)

cd %~dp0

rem IMPORTANT: this assumes all the *INFO.csv files have the following header:
rem
rem name,version,revision,url,license,copyright

set FIRST=yes
for %%i in (licenses\*INFO.csv) do (
	if [!FIRST!] == [yes] (
		set FIRST=no
		rem "|| rem": set errorlevel
		type %~dp0\%%i > %OUTPUT_FILE% || rem
		if ERRORLEVEL 1 (
			exit /b 1
		)
	) else (
		findstr /v "^name," %~dp0\%%i >> %OUTPUT_FILE%
	)
)

endlocal


