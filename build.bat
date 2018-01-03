REM This is just a helper script for building the ODBC driver in development.

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

cls
pushd .
cd %BUILD_DIR%

if "%1" == "1" (
	echo "Cleaning all build files"
	:: rm -r *
	del /s /q *
)

REM generate the MSVC project (.sln) and build
cmake ..
MSBuild .\ALL_BUILD.vcxproj /t:rebuild

dumpbin /exports .\Debug\elasticodbc.dll
if "%VSCMD_ARG_TGT_ARCH%" == "x64" (
	SET PSIZE=
) else (
	SET PSIZE=32
)
copy .\Debug\elasticodbc.dll %INSTALL_DIR%\elasticodbc%PSIZE%.dll

REM clear logs
echo.>%LOGGING_DIR%\mylog.txt
echo.>%LOGGING_DIR%\SQL32.LOG
echo.>%LOGGING_DIR%\SQL.LOG

popd
