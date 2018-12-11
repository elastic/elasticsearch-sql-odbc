# ODBC Driver for Elasticsearch SQL

This is the ODBC driver for Elasticsearch's SQL plugin.
The driver implements the Unicode version of the 3.80 API release.

## Elastic License Functionality

The driver files in this repository are subject to the Elastic License. The
full license can be found in [LICENSE.txt](LICENSE.txt). Usage requires a
[subscription](https://www.elastic.co/subscriptions). Files that are not
subject to the Elastic License are in the [libs](libs) directory.

## Supported platforms

The current target platform is Microsoft Windows, past and including version 7.
Full support will include Linux and OSX, on both x86 and amd64 architectures.

## Running Requirements

On the client side, the ODBC driver requires the OS'es Driver Manager to
interface between the clients and the driver itself.
On the server side, the requirements follow Elasticsearch SQL's requirements.

## Building Requirements

### CMake

The project is CMake enabled, which generates the environment-dependent build
pipeline. This is a general build requirement.

The building itself is then delegated to the platform-specific tools (MSVC or
make).

### External libraries/headers

The driver makes use of the following libraries/headers:

 * ODBC-Specification 
   - this is the project that currently contains the ODBC specification,
   including the headers defining the ODBC C API;
 * libcurl
   - the library is used for the HTTP(S) communication with Elasticsearch REST
   endpoint;
 * c-timestamp
   - the library is used for parsing the ISO 8601 formated timestamps received
   from Elasticsearch;
 * ujson4c
   - fast scanner library for JSON.

The required libraries are added as subtrees to the project, in the libs directory:
```
   somedirectory\
    |_elasticsearch-sql-odbc
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


### Windows

#### MSVC 

Building the driver requires the installation of Microsoft tools. These can be
from the Visual Studio pack or with the [standalone tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).

Version 2017 Enterprize 15.5.2 is used to develop with, older versions
should work fine too, with their corresponding modules. The lists of packages
for MSVC 2017 are given below.

Required packages:

 * MSBuild
   - as the build tool;
 * Windows 10 SDK
   - headers and libraries;
 * VC++ toolset
   - for the compiler;
 * C++/CLI support
   - for the DSN editor C to C# CLI binding;
 * C# support
   - for the DSN editor C# form;
 * F# support
   - for the MSI packaging.

Optional packages:

 * CMake Project Wizards
 * C++ profiling tools
 * Git for Windows / GitHub Extentions for VisualStudio

#### Powershell

Powershell is currently used to automatically generate the definition file of
the symbols to export in the driver DLL. The definition file is required,
since Windows' shared objects symbols are by default not exported.


## Building

### Windows MSVC

Start Visual Studio and menu-access File > Open > "CMake..". Navigate to the checkout folder of the project and select the file CMakeLists.txt. 
Once the file is read and imported, a new menu top item will appear, CMake. Menu-access CMake > "BuildAll".


### Windows CLI 

The project contains a BAT script - ```build.bat``` - that can run different
steps for building the ODBC driver.

Some environment parameters can be set to customized its behavior (see start
of script).

The script can also take a set of parameters, run ```build.bat help``` to see
what they mean. ```build.bat``` will build the driver itself, by invoking
CMake and MSBuild, as needed. ```build.bat proper``` will clean the project to initial state. ```build.bat all tests``` will run the unit tests.

## Installation

See: https://www.elastic.co/guide/en/elasticsearch/sql-odbc/current/index.html
