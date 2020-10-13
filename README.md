# ODBC Driver for Elasticsearch SQL

This is the ODBC driver for Elasticsearch's SQL plugin.
The driver implements the Unicode version of the 3.80 API release.

## Elastic License Functionality

The driver files in this repository are subject to the Elastic License. The
full license can be found in [LICENSE.txt](LICENSE.txt). Usage requires a
[subscription](https://www.elastic.co/subscriptions). Files that are not
subject to the Elastic License are in the [libs](libs) directory.

## Supported platforms

The currently supported platforms on both x86 and amd64 architectures are:

- Microsoft Windows 10
- Microsoft Windows Server 2016

Support for other platforms might be added at a later time.

## Running Requirements

On the client side, the ODBC driver requires the OS'es Driver Manager to
interface between the clients and the driver itself.
On the server side, the requirements follow Elasticsearch SQL's requirements.

## Building Requirements

### CMake

The project is CMake enabled, which generates the environment-dependent build
pipeline. This is a general build requirement.

The building itself is then delegated to the platform-specific tools.

CMake 3.14 or newer is required for building with Visual Studio 2019.

### External libraries/headers

The driver makes use of the following libraries/headers:

 * ODBC-Specification 
   - this is the project that contains the ODBC specification, including the
   headers defining the ODBC C API;
 * libcurl
   - the library is used for the HTTP(S) communication with Elasticsearch REST
   API;
 * c-timestamp
   - library used for parsing ISO 8601 formated timestamps;
 * ujson4c
   - fast scanner library for JSON;
 * tinycbor
   - a small CBOR encoder and decoder library.

The required libraries are added as subtrees to the project, in the libs directory:
```
   somedirectory\
    |_elasticsearch-sql-odbc
      |_CMakeLists.txt
      |_...
      |_libs
        |_ODBC-Specification
        |_curl
        |_zlib
        |_c-timestamp
        |_ujson4c
        |_tinycbor
```


### Windows

#### MSVC 

Building the driver requires the installation of Microsoft tools. These can be
from the Visual Studio pack or with the [standalone tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).

Version 2019 Enterprise 16 is used to develop with, older versions
should work fine too, with their corresponding modules. The lists of packages
for MSVC 2019 are given below.

Required packages:

 * MSBuild
   - as the build tool;
 * Windows 10 SDK
   - headers and libraries;
 * VC++ toolset
   - for the compiler;
 * C++/CLI support
   - for DSN editor's C-to-C# CLI binding;
 * C# support
   - for DSN editor's C# form;
 * F# support
   - for building the MSI package.

Optional packages:

 * CMake Project Wizards
 * C++ profiling tools
 * Git for Windows / GitHub Extentions for VisualStudio

## Building

### Windows MSVC

Start Visual Studio and menu-access File > Open > "CMake..". Navigate to the checkout folder of the project and select the file CMakeLists.txt. 
Once the file is read and imported, a new menu top item will appear, CMake. Menu-access CMake > "BuildAll".


### Windows CLI 

The project contains a BAT script - ```build.bat``` - that can run different
steps for building the ODBC driver.

Some environment parameters can be set to customized its behavior (see start
of script).

The script will take a set of parameters, run ```build.bat help``` to see
which these are.

## Testing

Testing the driver is done with unit tests and integration tests.

### Requirements

The unit testing makes use of the Googletest framework. This is being fetched and built at testing time.

The integration testing makes use of a Python application that requires the
following packages be installed:

 * Python3, both x86 and amd64 distributions
   - both x86 and x64 driver builds are tested;
 * Python launcher (py)
   - to selectively launch the right Python build;

For each of the two Python releases, the following packages must be installed:

 * pyodbc
   - for ODBC access;
 * requests
   - for HTTP support;
 * psutils
   - for OS process management.

## Installation

See: https://www.elastic.co/guide/en/elasticsearch/reference/current/sql-odbc.html
