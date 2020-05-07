#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

# 3.1.0: CMAKE_GENERATOR_PLATFORM
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

set(DRIVER_BASE_NAME elasticodbc CACHE STRING
	"The base name of the driver")

# driver's version
set(DRV_VERSION 7.9.0)
set(VERSION_QUALIFIER $ENV{VERSION_QUALIFIER} CACHE STRING
	"Extra string to append to the install directory name")

# build a UNICODE driver? (the only supported way currently)
set(IS_UNICODE 1)
# should the ODBC driver's name respect the Short Format Name?
set(SFN_DRV_NAME 1)

#include(GenerateExportHeader)

if (${WIN32})
	# or: CMAKE_SIZEOF_VOID_P EQUAL 8
	if (${CMAKE_GENERATOR_PLATFORM} MATCHES [Xx]64)
		set(TARCH x64) # target arch
		set(BARCH ) # bits architecture (64 is the default, silent)
		set(PLAT_ARCH windows-x86_64)
	else (${CMAKE_GENERATOR_PLATFORM} MATCHES [Xx]64)
		set(TARCH x86)
		set(BARCH 32)
		set(PLAT_ARCH windows-x86)
	endif (${CMAKE_GENERATOR_PLATFORM} MATCHES [Xx]64)
	message("Building for Windows, ${TARCH}.")
else (${WIN32})
	message(FATAL_ERROR "No support for current platform yet")
endif (${WIN32})

# activate version handling by the project
cmake_policy(SET CMP0048 NEW)
# explicit languages support (Cs are defaults)
project(${DRIVER_BASE_NAME}
	VERSION ${DRV_VERSION}
	LANGUAGES CXX C)

if (${IS_UNICODE})
	set(ENCODING u) # Unicode
	set(ENCODING_VERBOSE Unicode)
else (${IS_UNICODE})
	set(ENCODING a) # ANSI
	set(ENCODING_VERBOSE ANSI) # ANSI
endif (${IS_UNICODE})

# driver name (ANSI/Unicode x 32/64)
if (${SFN_DRV_NAME})
	set(DRV_NAME "${DRIVER_BASE_NAME}${PROJECT_VERSION_MAJOR}${ENCODING}")
else (${SFN_DRV_NAME})
	set(DRV_NAME "${DRIVER_BASE_NAME}${PROJECT_VERSION_MAJOR}${ENCODING}${BARCH}")
endif (${SFN_DRV_NAME})

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# which version is the code at?
execute_process(COMMAND
	git describe --dirty=+ --broken=X --always --tags
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	RESULT_VARIABLE CMD_RETURN
	OUTPUT_VARIABLE CMD_OUTPUT
	ERROR_VARIABLE CMD_OUTERR
	)
#	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}"
if (${CMD_RETURN})
	message(WARNING "Git command failed:")
	message("\tret: ${CMD_RETURN}")
	message("\tout: ${CMD_OUTPUT}")
	message("\terr: ${CMD_OUTERR}")
	set(DRV_SRC_VER "n/a")
else (${CMD_RETURN})
	set(DRV_SRC_VER ${CMD_OUTPUT})
endif (${CMD_RETURN})

# Set compiler flags and options.
if (${WIN32})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
	# set the Visual Studio warning level to 4
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
	# TODO: review@alpha
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D_CRT_NONSTDC_NO_DEPRECATE")
	# don't complain about non-constant aggregate initializer (C4204)
	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Ze")
	# parallel building
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

	# 2MB stack size
	# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:2097152")
	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /F 2097152")
	if ("${BARCH}" EQUAL "32")
		# Force UJSON4C's Unicode decoder scratchpad onto the heap. Its large
		# default size (128K) leads to stack overflow with some (older) apps.
		set(CMAKE_C_FLAGS
			# defined to one wchar_t/Win/UTF-16
			"${CMAKE_C_FLAGS} /DJSON_MAX_STACK_BUFFER_SIZE=2")
	endif ("${BARCH}" EQUAL "32")

	if (${IS_UNICODE})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DUNICODE /D_UNICODE")
	endif (${IS_UNICODE})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_NAME=${DRV_NAME}")

	#
	# set the version identifiers
	#
	set(CMAKE_C_FLAGS
		"${CMAKE_C_FLAGS} /DDRV_VERSION=${DRV_VERSION}${VERSION_QUALIFIER}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_SRC_VER=${DRV_SRC_VER}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_ENCODING=${ENCODING}")
	# ...including the build type indicator
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /DDRV_BUILD_TYPE=d")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /DDRV_BUILD_TYPE=r")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO
		"${CMAKE_C_FLAGS_RELWITHDEBINFO} /DDRV_BUILD_TYPE=i")
	set(CMAKE_C_FLAGS_MINSIZEREL
		"${CMAKE_C_FLAGS_MINSIZEREL} /DDRV_BUILD_TYPE=s")

	# unless building for stripping Release, export the testing functions
	# (this will allow RelWithDebInfo buliding and still be able to test it)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /DTEST_API=")
	set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /DTEST_API=")

	# Only used when a long logging message is required for troubleshooting
	# (like one entire server answer).
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DWITH_EXTENDED_BUFF_LOG")
	# Account for the time the process spends within the driver.
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DWITH_OAPI_TIMING")
else (${WIN32})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif (${WIN32})

# driver's source dir
set(DRV_SRC_DIR driver)
aux_source_directory(${DRV_SRC_DIR} DRV_SRC)

# configure Module definition file (symbols to export)
configure_file(${CMAKE_SOURCE_DIR}/driver/driver.def.cmake ${CMAKE_BINARY_DIR}/${DRV_NAME}.def)

# generate installable NOTICE.txt
execute_process(COMMAND
	${CMAKE_SOURCE_DIR}/devtools/3rd_party/notice_report.bat NOTICE.txt
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
	RESULT_VARIABLE CMD_RETURN
	OUTPUT_VARIABLE CMD_OUTPUT
	ERROR_VARIABLE CMD_OUTERR
	)
#	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}"
if (${CMD_RETURN})
	message(WARNING "Generating install NOTICE.txt file failed:")
	message("\tret: ${CMD_RETURN}")
	message("\tout: ${CMD_OUTPUT}")
	message("\terr: ${CMD_OUTERR}")
endif (${CMD_RETURN})


#
# add ODBC-Specification to the project
#
set(ODBC_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/ODBC-Specification CACHE PATH
	"ODBC-Specification source path")
set(ODBC_INC ${ODBC_PATH_SRC}/Windows/inc)


#
# add ujson4c to the project
#
set(UJSON4C_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/ujson4c CACHE PATH
	"Lib ujson4c source path")
aux_source_directory(${UJSON4C_PATH_SRC}/src DRV_SRC)
aux_source_directory(${UJSON4C_PATH_SRC}/3rdparty DRV_SRC)
set(UJSON4C_INC ${UJSON4C_PATH_SRC}/src ${UJSON4C_PATH_SRC}/3rdparty )

#
# add c-timestamp to the project
#
set(CTIMESTAMP_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/c-timestamp CACHE PATH
	"Lib c-timestamp source path")
aux_source_directory(${CTIMESTAMP_PATH_SRC}/ DRV_SRC)

#
# add libcurl (and zlib) to the project
#
set(LIBCURL_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/curl CACHE PATH
	"Lib curl source path")
set(LIBCURL_BUILD_TYPE debug CACHE STRING
	"Lib curl build type: debug (default) or release")

if (${LIBCURL_BUILD_TYPE} MATCHES [Dd][Ee][Bb][Uu][Gg])
	set(LIBCURL_DEBUG_ENABLED yes)
	set(LIBCURL_BUILD_TYPE debug)
	set(LIBCURL_BUILD_SUFFIX _debug)
	# zlib's nmake-based "win32" build system contains a default,
	# non-modifiable "-MD". Setting ZLIB_LOC will override it, which will
	# issue a compiler warning (that can be ignored, can't be supressed).
	set(ZLIB_LOC -MDd)
else (${LIBCURL_BUILD_TYPE} MATCHES [Dd][Ee][Bb][Uu][Gg])
	set(LIBCURL_DEBUG_ENABLED no)
	set(LIBCURL_BUILD_TYPE release)
	# empty LIBCURL_BUILD_SUFFIX
	# empty ZLIB_LOC
endif (${LIBCURL_BUILD_TYPE} MATCHES [Dd][Ee][Bb][Uu][Gg])

set(LIBCURL_LD_PATH
	# Curl "installs" the .dll and .lib in different directories -> use the
	# build dir to find both files in same directory instead of installing.
	# Curl's win build root directory is not configurable.
	# The path built below is only constant for the subtree tag and the nmake
	# options below: ZLIB, IPv6, SSPI, WinSSL, plus IDN, not echoed in name.
	${LIBCURL_PATH_SRC}/builds/libcurl-vc-${TARCH}-${LIBCURL_BUILD_TYPE}-static-zlib-static-ipv6-sspi-winssl-obj-lib/
	CACHE PATH "Lib curl load library path")
set(LIBCURL_INC_PATH ${LIBCURL_PATH_SRC}/include CACHE PATH
	"Lib curl include path")

# zlib paths
set(ZLIB_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/zlib CACHE PATH
	"Lib zlib source path")
set(ZLIB_PATH_INST ${LIBCURL_PATH_SRC}/builds/zlib-${TARCH} CACHE PATH
	"Lib zlib install path")

# Build zlib, then libcurl.
# Note: this happens at config time as a pre-requisite, for now. This might
# be changed to a build target later (possibly as a CMake subproject: re-link
# only if out-of-date, skip building the .exe, allow disabling non-HTTP
# protos, || build, setting output destination).
# Building as a pre-requisite has however the disadvantage of making the
# entire build "single-config", since the build type (rel/dbg) is decided at
# CMake-generation, not along the MSBuild invocation.
if (NOT IS_DIRECTORY ${LIBCURL_LD_PATH})
	# build zlib first.
	message("Building zlib library in ${ZLIB_PATH_SRC}")
	execute_process(COMMAND
		# zlib's "win32" makefile builds in situ => always clean before
		# building, potentially removing builds of different architecture than
		# current's build.
		nmake /f win32/Makefile.msc clean zlib.lib LOC=${ZLIB_LOC}
		RESULT_VARIABLE CMD_RETURN
		WORKING_DIRECTORY "${ZLIB_PATH_SRC}"
		)
	if (${CMD_RETURN})
		message(FATAL_ERROR "Building zlib failed.")
	endif (${CMD_RETURN})
	# libcurl expects a /lib and a /include folder under the location provided
	# as the path to zlib. zlib/win32's makefile has no install target, so
	# we'll just cmake-install them (under libcurl's build folder).
	file(INSTALL ${ZLIB_PATH_SRC}/zlib.lib DESTINATION ${ZLIB_PATH_INST}/lib)
	file(GLOB ZLIB_H_FILES LIST_DIRECTORIES false ${ZLIB_PATH_SRC}
		${ZLIB_PATH_SRC}/*.h)
	file(INSTALL ${ZLIB_H_FILES} DESTINATION ${ZLIB_PATH_INST}/include)

	# build libcurl second: config first, build afterwards..
	message("Building curl library in ${LIBCURL_PATH_SRC}")
	execute_process(COMMAND buildconf.bat
		RESULT_VARIABLE CMD_RETURN
		WORKING_DIRECTORY "${LIBCURL_PATH_SRC}"
		)
	if (NOT ${CMD_RETURN})
		execute_process(COMMAND
			nmake /f Makefile.vc mode=static MACHINE=${TARCH}
			ENABLE_WINSSL=yes ENABLE_IDN=yes ENABLE_IPV6=yes ENABLE_SSPI=yes
			WITH_ZLIB=static ZLIB_PATH=${ZLIB_PATH_INST}
			# build type needs to be synchronized (to link in the same CRT)
			DEBUG=${LIBCURL_DEBUG_ENABLED}
			# This "sneaks in" a define to disable all other protocols than
			# HTTP in libcurl. There's currently (7.61.1) no way of doing this
			# on Windows, without patching library's code/makefiles (or using
			# currently still "poorly maintained" CMake generation).
			# Ideally, one would use ${CMAKE_CXX_COMPILER} here. However,
			# curl's makefile simply expects a 'cl.exe' available in the PATH.
			"CC=cl.exe /DHTTP_ONLY"
			RESULT_VARIABLE CMD_RETURN
			WORKING_DIRECTORY "${LIBCURL_PATH_SRC}/winbuild"
			)
	endif (NOT ${CMD_RETURN})
	if (${CMD_RETURN})
		message(FATAL_ERROR "Building libcurl failed.")
	endif (${CMD_RETURN})
endif(NOT IS_DIRECTORY ${LIBCURL_LD_PATH})

# add libcurl as dependency
if (${WIN32})
	add_library(zlib STATIC IMPORTED)
	set_property(TARGET zlib PROPERTY IMPORTED_LOCATION
		${ZLIB_PATH_INST}/lib/zlib.lib)

	add_library(libcurl STATIC IMPORTED)
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION
		${LIBCURL_LD_PATH}/libcurl_a${LIBCURL_BUILD_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DCURL_STATICLIB")
	# Libraries that libcurl/WinSSL links against.
	# Removed: wldap32 advapi32 gdi32 user32 (unused with current config)
	set(LIBCURL_WIN_LIBS ws2_32 crypt32 normaliz)
else (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION
		${LIBCURL_LD_PATH}/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX})
endif (${WIN32})

add_custom_target(curlclean
	COMMAND nmake /f Makefile.vc mode=static clean
	COMMAND ../buildconf.bat -clean
		WORKING_DIRECTORY "${LIBCURL_PATH_SRC}/winbuild"
	)
add_custom_target(zlibclean
	COMMAND nmake /f win32/Makefile.msc clean
		WORKING_DIRECTORY "${ZLIB_PATH_SRC}"
	)

#
# add tinycbor to the project
#
set(TINYCBOR_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/tinycbor CACHE PATH
	"Lib tinycbor source path")
file(COPY ${TINYCBOR_PATH_SRC}/src/cborparser.c DESTINATION
	${CMAKE_BINARY_DIR})
# tinycbor doesn't expose (yet? #125) the text/binary string pointer, since the
# string can span multiple stream chunks. However, in our case the CBOR object
# is available entirely, so access to it can safely be had; this saves a
# superfluous allocation/copy. FIXME -- lib PR, proper exposure.
file(APPEND ${CMAKE_BINARY_DIR}/cborparser.c
"
CborError cbor_value_get_string_chunk(CborValue *it,
	const void **bufferptr, size_t *len)
{
	CborError err = get_string_chunk(it, bufferptr, len);
	return err != CborNoError ? err : preparse_next_value(it);
}")
list(APPEND DRV_SRC ${CMAKE_BINARY_DIR}/cborparser.c)
list(APPEND DRV_SRC ${TINYCBOR_PATH_SRC}/src/cborvalidation.c)
list(APPEND DRV_SRC ${TINYCBOR_PATH_SRC}/src/cborerrorstrings.c)
list(APPEND DRV_SRC ${TINYCBOR_PATH_SRC}/src/cborencoder.c)
list(APPEND DRV_SRC ${TINYCBOR_PATH_SRC}/src/cborparser_dup_string.c)
list(APPEND DRV_SRC
	${TINYCBOR_PATH_SRC}/src/cborencoder_close_container_checked.c)
set(TINYCBOR_INC ${TINYCBOR_PATH_SRC}/src)
# limit how deep the parser will recurse (current need: 3)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DCBOR_PARSER_MAX_RECURSIONS=16")

#
# Patch installer's and editor's AssemblyInfos with the version being built
#
execute_process(COMMAND
	${CMAKE_SOURCE_DIR}/installer/build.bat patchversions ${DRV_VERSION}${VERSION_QUALIFIER}
	RESULT_VARIABLE CMD_RETURN
	OUTPUT_VARIABLE CMD_OUTPUT
	ERROR_VARIABLE CMD_OUTERR
	)
if (${CMD_RETURN})
	message(WARNING "Patching installer with the versions failed:")
	message("\tret: ${CMD_RETURN}")
	message("\tout: ${CMD_OUTPUT}")
	message("\terr: ${CMD_OUTERR}")
endif (${CMD_RETURN})

#
# DSN Config GUI
#
set(DSNBND_LIB_BIN_DIR_BASE ${CMAKE_BINARY_DIR}/dsneditor-${TARCH})
set(DSNEDITOR_INC_PATH ${CMAKE_SOURCE_DIR}/dsneditor/EsOdbcDsnBinding/)

# Generate resource file for the editor binding.
# The directory where .vcxproj defines it isn't yet created, though: that
# happens later when the "dsneditor" custom command below is executed =>
# generate it in the general build directory for now and copy it part of the
# custom command.
configure_file(${CMAKE_SOURCE_DIR}/dsneditor/EsOdbcDsnBinding/EsOdbcDsnBinding.rc.cmake ${CMAKE_BINARY_DIR}/EsOdbcDsnBinding.rc)

file(TO_NATIVE_PATH ${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/
	DSNBND_LIB_BIN_DIR_BASE_NATIVE)
add_custom_target(dsneditor
	COMMAND xcopy /Y/F EsOdbcDsnBinding.rc ${DSNBND_LIB_BIN_DIR_BASE_NATIVE}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	COMMAND MSBuild ${CMAKE_SOURCE_DIR}/dsneditor/EsOdbcDsnBinding/EsOdbcDsnBinding.vcxproj
		# place all the build items (by- & products) into a single dir
		/p:OutputPath=${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/
		/p:IntermediateOutputPath=${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/
		/p:OutDir=${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/
		/p:IntDir=${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/
		/p:Configuration=$<CONFIG>
		 /p:Platform=${TARCH} # make it explicit, for x-arch builds
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	COMMENT "Building the DSN editor libraries.")

#
# finally, set destination library
#
# generate resource file
configure_file(${DRV_SRC_DIR}/driver.rc.cmake ${CMAKE_BINARY_DIR}/driver.rc)
add_library(${DRV_NAME} SHARED ${DRV_SRC} ${CMAKE_BINARY_DIR}/${DRV_NAME}.def
	${CMAKE_BINARY_DIR}/driver.rc)
#generate_export_header(${DRV_NAME})
target_compile_definitions(${DRV_NAME} PRIVATE "DRIVER_BUILD")
add_dependencies(${DRV_NAME} dsneditor)
include_directories(${ODBC_INC} ${DRV_SRC_DIR} ${LIBCURL_INC_PATH}
	${UJSON4C_INC} ${CTIMESTAMP_PATH_SRC} ${TINYCBOR_INC}
	${DSNEDITOR_INC_PATH})
target_link_libraries(${DRV_NAME} odbccp32 legacy_stdio_definitions
	${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/esdsnbnd${BARCH}${CMAKE_IMPORT_LIBRARY_SUFFIX}
	zlib libcurl ${LIBCURL_WIN_LIBS})


#
# Set up the testing project/target
#
enable_testing()
# ... and testing directory to build
add_subdirectory(test)


#
# Set up the instalation
#
set(INSTALL_DIR
	${DRIVER_BASE_NAME}-${DRV_VERSION}${VERSION_QUALIFIER}-${PLAT_ARCH}
	# this must remain a STRING (i.e not PATH), otherwise CPACK won't work
	CACHE STRING
	"Directory to install the driver files into")

# this will cause the 'install' to no longer depend on 'all', which avoids
# building the tests, but this also means that the library must be built
# before building the 'install' target.
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
# add instalation project/target
install(FILES
	LICENSE.rtf LICENSE.txt ${CMAKE_BINARY_DIR}/NOTICE.txt
	DESTINATION ${INSTALL_DIR})
# add libcurl if build dynamically
# add editor DLLs
install(FILES
	${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/esdsnedt${CMAKE_SHARED_LIBRARY_SUFFIX}
	${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/esdsnbnd${BARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}
	DESTINATION ${INSTALL_DIR})
install(TARGETS ${DRV_NAME}
	DESTINATION ${INSTALL_DIR})

#
# Set up the packaging
#
set(CPACK_GENERATOR "ZIP")
# don't build ALL (but see CMAKE_SKIP_INSTALL_ALL_DEPENDENCY comment)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY TRUE)
set(CPACK_PACKAGE_FILE_NAME
	${DRIVER_BASE_NAME}-${DRV_VERSION}${VERSION_QUALIFIER}-${PLAT_ARCH})
include(CPack)


#
# Generation summary
#
message("  ***** <Generation summary>")
message("Driver source files: ${DRV_SRC}")
message("Driver compile flags: ${CMAKE_C_FLAGS}")
message("Driver include paths: ${ODBC_INC} ${DRV_SRC_DIR}"
	"${LIBCURL_INC_PATH} ${UJSON4C_INC} ${CTIMESTAMP_PATH_SRC}"
	"${DSNEDITOR_INC_PATH}")
# there's no var for this
message("Driver link libraries: odbccp32 legacy_stdio_definitions esdsnbnd"
	"libcurl ${LIBCURL_WIN_LIBS}")
message("Driver install target: ${INSTALL_DIR}.")
message("Driver packaging target: ${CPACK_PACKAGE_FILE_NAME}.")
message("   ***** </Generation summary>")

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 :
