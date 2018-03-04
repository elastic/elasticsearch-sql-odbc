# ODBC Driver for Elastic SQL

This is the ODBC driver for Elastic's SQL engine.
The driver implements the Unicode version of the 3.80 API release.

## Supported platforms

The current target platform is Microsoft Windows, past and including version 7. The 32bits version of the driver is currently developped.
Full support should include Linux and OSX, on both x86 and amd64 architectures.

## Running Requirements

On the client side, the ODBC driver requires the OS'es Driver Manager to
interface between the clients and the driver itself.
On the server side, the requirements follow Elastic SQL's requirements.

## Building Requirements

### CMake

The project is CMake enabled, which generates the environment-dependent build
pipeline. This is a general build requirement.
CMake version: 2.8.6 or later.

The building itself is then delegated to the platform-specific tools (MSVC or
make).

### External libraries/headers

The driver makes use of the following libraries/headers:

 * ODBC-Specification : this is the project that currently contains the ODBC
   specification, including the headers defining the ODBC C API.
 * libcurl : the library is used for the HTTP(S) communication with
   Elasticsearch REST endpoints.
 * c-timestamp : the library is used for parsing the ISO 8601 formated
   timestamps received from Elasticsearch.
 * ujson4c : fast scanner library for JSON.


### Windows

#### MSVC 

Building the driver requires the installation of Microsoft Visual Studio. 
Version 2017 Enterprize 15.5.2 is used to develop against, older versions
should work fine too, with their corresponding modules. The lists of packages
for MSVC 2017 are given below.

Required packages:

 * Windows [10] SDK for UWP: C++
 * C++/CLI support
 * MSBuild

Optional packages:

 * CMake Project Wizards
 * C++ profiling tools
 * Git for Windows / GitHub Extentions for VisualStudio

#### Powershell

Powershell is currently used to automatically generate the definition file of
the symbols to export in the driver DLL. The definition file is required,
since Windows' shared objects symbols are by default not exported.


## Building

### Libraries

The libraries need to be cloned locally and the indicated revision and one
built, as indicated below.
The cloning can be done arbitrarily on the disk and then the corresponding
paths indicated in environment variables. The simplest way though is to export
them in driver's libs directory, where the build script expects them by
default:
```
   somedirectory\
    |_x-pack-odbc
      |_README.md
      |_CMakeLists.txt
      |_build.bat
      |_driver
      |_builds
      |_libs
        |_ODBC-Specification
        |_curl
        |_c-timestamp
        |_ujson4c
```

#### ODBC-Specification

Clone the project *https://github.com/Microsoft/ODBC-Specification.git*.
The headers is revised with C defines, so the project can be left at *master*
revision.


#### libcurl

The library needs to be exported and compiled, since the driver will be linked
against it. The driver has been tested against release 7.58.0 of the library.

The following steps will export and build the library (for a 64 bits build):
```
  > cd libs
  > git clone https://github.com/curl/curl.git
  > cd curl
  > git checkout curl-7_58_0
  > .\buildconf.bat
  > cd .\winbuild\
  > nmake /f Makefile.vc mode=dll MACHINE=x64
  > dir ..\builds\libcurl-vc-x64-release-dll-ipv6-sspi-winssl\bin
```
If the build was succesfull, the last step will show the libcurl.dll file.

#### c-timestamp

Clone the project *https://github.com/chansen/c-timestamp.git*.
Current (Feb 2018) *master* branch should be used.

#### ujson4c

Clone the project *https://github.com/esnme/ujson4c.git*.
Current (Feb 2018) *master* branch should be used. The library contains two
currently unpatched defects ([[9]](https://github.com/esnme/ujson4c/issues/9),
[[10]](https://github.com/esnme/ujson4c/issues/10)). Driver's top directory
contains a patch *ujson4c.diff* that needs to be applied before building the
driver; using GnuWin *patch* utility (that handles well Win's CRLF):
```
  > patch -p1 -i .\ujson4c.diff -d .\ujson4c
```

### Windows MSVC

Start Visual Studio and menu-access File > Open > "CMake..". Navigate to the checkout folder of the project and select the file CMakeLists.txt. 
Once the file is read and imported, a new menu top item will appear, CMake. Menu-access CMake > "BuildAll".


### Windows CLI 

The project contains a simple BAT script - build.bat - that can be invoked and
that will sequentially execute CMake and MSBuild. Some environment parameters
can be customized (see start of the script).

## Installation

The driver will be provided with the Windows installer for Elastic X-Pack.
