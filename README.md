# ODBC Driver for Elastic SQL

This is the ODBC driver for Elastic's SQL engine.
The driver implements the Unicode version of the 3.80 API release.

## Supported platforms

The current target platform is Microsoft Windows, past and including version 7. The 32bits version of the driver is currently developped.
Full support should include Linux and OSX, on both x86 and amd64 architectures.

## Running Requirements

On the client side, the ODBC driver requires the OS'es Driver Manager to interfae between the clients and the driver itself.
On the server side, the requirements follow Elastic SQL's requirements.

## Building Requirements

### CMake

The project is CMake enabled, which generates the environment-dependent build pipeline. This is a general build requirement.
CMake version: 2.8.6 or later.

The building itself is then delegated to the platform-specific tools (MSVC or make).

### Windows

#### MSVC 

Building the driver requires the installation of Microsoft Visual Studio. 
Version 2017 Enterprize 15.5.2 is used to develop against, older versions should work fine too, with their corresponding modules. The lists of packages for MSVC 2017 are given below.

Required packages:

 * Windows [10] SDK for UWP: C++
 * C++/CLI support
 * MSBuild

Optional packages:

 * CMake Project Wizards
 * C++ profiling tools
 * Git for Windows / GitHub Extentions for VisualStudio

#### Powershell

Powershell is currently used to automatically generate the definition file of the symbols to export in the driver DLL. The definition file is required, since Windows' shared objects symbols are by default not exported.


## Building

### Windows MSVC

Start Visual Studio and menu-access File > Open > "CMake..". Navigate to the checkout folder of the project and select the file CMakeLists.txt. 
Once the file is read and imported, a new menu top item will appear, CMake. Menu-access CMake > "BuildAll".


### Windows CLI 

The project contains a simple BAT script that can be invoked and that will sequentially execute CMake and MSBuild. Some environment parameters would need to be customized (see start of the script).

## Installation

The driver will be provided with the Windows installer for Elastic X-Pack.
