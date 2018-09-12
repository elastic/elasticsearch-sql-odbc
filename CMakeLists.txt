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
set(DRV_VERSION 7.0.0)
set(VERSION_QUALIFIER $ENV{VERSION_QUALIFIER} CACHE STRING
	"Extra string to append to the install directory name")

# build a UNICODE driver? (the only supported way currently)
set(IS_UNICODE 1)

#include(GenerateExportHeader)

if (${WIN32})
	if (${CMAKE_GENERATOR_PLATFORM} MATCHES x64)
		set(TARCH x64) # target arch
		set(BARCH ) # bits architecture (64 is the default, silent)
		set(PLAT_ARCH windows-x86_64)
	else (${CMAKE_GENERATOR_PLATFORM} MATCHES x64)
		set(TARCH x86)
		set(BARCH 32)
		set(PLAT_ARCH windows-x86)
	endif (${CMAKE_GENERATOR_PLATFORM} MATCHES x64)
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
set(DRV_NAME "${DRIVER_BASE_NAME}${PROJECT_VERSION_MAJOR}${ENCODING}${BARCH}")

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
	# don't complain about non-constant aggregate initializer (C4204)
	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Ze")
	# parallel building
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

	# 2MB stack size
	# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:2097152")
	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /F 2097152")

	if (${IS_UNICODE})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DUNICODE /D_UNICODE")
	endif (${IS_UNICODE})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_NAME=${DRV_NAME}")

	#
	# set the version identifiers
	#
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_VERSION=${DRV_VERSION}")
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

	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DNDEBUG")
else (${WIN32})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif (${WIN32})
message("C flags: ${CMAKE_C_FLAGS} .")

# driver's source dir
set(DRV_SRC_DIR driver)
aux_source_directory(${DRV_SRC_DIR} DRV_SRC)

# generate Module definition file (symbols to export)
execute_process(COMMAND
	${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}/build_def.bat ${DRV_NAME}
		${CMAKE_BINARY_DIR}/${DRV_NAME}.def
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}"
	RESULT_VARIABLE CMD_RETURN
	OUTPUT_VARIABLE CMD_OUTPUT
	ERROR_VARIABLE CMD_OUTERR
	)
#	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}"
if (${CMD_RETURN})
	message(WARNING "Generating module .def file failed:")
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
# add libcurl to the project
#
set(LIBCURL_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/curl CACHE PATH
	"Lib curl source path")
set(LIBCURL_LD_PATH
	# Curl "installs" the .dll and .lib in different directories -> use the
	# build dir to find both files in same directory instead of installing.
	# Curl's win build root directory is not configurable.
	# The path built below is only constant for the subtree tag (current:
	# 7.61.0) and it's default nmake options (only with: IPv6, SSPI, WinSSL).
	${LIBCURL_PATH_SRC}/builds/libcurl-vc-${TARCH}-release-dll-ipv6-sspi-winssl-obj-lib/
	CACHE PATH "Lib curl load library path")
set(LIBCURL_INC_PATH ${LIBCURL_PATH_SRC}/include CACHE PATH
	"Lib curl include path")
if (NOT IS_DIRECTORY ${LIBCURL_LD_PATH})
	# Note: building libcurl happens at config time (as a pre-requisite), for
	# now. This might be changed to a build target later.
	# TODO: add non-WiN32 building.
	execute_process(COMMAND buildconf.bat
		WORKING_DIRECTORY "${LIBCURL_PATH_SRC}"
		)
	if (NOT ${CMD_RETURN})
		execute_process(COMMAND nmake /f Makefile.vc mode=dll MACHINE=${TARCH}
			WORKING_DIRECTORY "${LIBCURL_PATH_SRC}/winbuild"
			)
	endif (NOT ${CMD_RETURN})
	if (${CMD_RETURN})
		message(FATAL_ERROR "Building libcurl failed:")
	endif (${CMD_RETURN})
endif(NOT IS_DIRECTORY ${LIBCURL_LD_PATH})
# add libcurl as dependency
add_library(libcurl SHARED IMPORTED)
if (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION
		${LIBCURL_LD_PATH}/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX})
	set_property(TARGET libcurl PROPERTY IMPORTED_IMPLIB
		${LIBCURL_LD_PATH}/libcurl${CMAKE_STATIC_LIBRARY_SUFFIX})
else (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION
		${LIBCURL_LD_PATH}/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX})
endif (${WIN32})

add_custom_target(curlclean
	COMMAND nmake /f Makefile.vc mode=dll clean
	WORKING_DIRECTORY "${LIBCURL_PATH_SRC}/winbuild"
	)

message("Driver source files: ${DRV_SRC} .")
message("Driver include paths: " ${ODBC_INC} ${DRV_SRC_DIR}
	${LIBCURL_INC_PATH} ${UJSON4C_INC} ${CTIMESTAMP_PATH_SRC})

# generate resource file
configure_file(${DRV_SRC_DIR}/driver.rc.cmake ${CMAKE_BINARY_DIR}/driver.rc)

#
# finally, set destination library
#
add_library(${DRV_NAME} SHARED ${DRV_SRC} ${CMAKE_BINARY_DIR}/${DRV_NAME}.def
	${CMAKE_BINARY_DIR}/driver.rc)
#generate_export_header(${DRV_NAME})
target_compile_definitions(${DRV_NAME} PRIVATE "DRIVER_BUILD")

include_directories(${ODBC_INC} ${DRV_SRC_DIR} ${LIBCURL_INC_PATH}
	${UJSON4C_INC} ${CTIMESTAMP_PATH_SRC})

target_link_libraries(${DRV_NAME} odbccp32 legacy_stdio_definitions libcurl)

# add testing project/target
enable_testing()
# ... and testing directory to build
add_subdirectory(test)

set(INSTALL_DIR
	${DRIVER_BASE_NAME}-${DRV_VERSION}${VERSION_QUALIFIER}-${PLAT_ARCH}
	# this must remain a STRING (i.e not PATH), otherwise CPACK won't work
	CACHE STRING
	"Directory to install the driver files into")

message("Install target: ${INSTALL_DIR}.")
# this will cause the 'install' to no longer depend on 'all', which avoids
# building the tests, but this also means that the library must be built
# before building the 'install' target.
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
# add instalation project/target
install(FILES
	LICENSE.rtf LICENSE.txt
	# need to use FILE : https://public.kitware.com/Bug/view.php?id=14311
	${LIBCURL_LD_PATH}/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX}
	DESTINATION ${INSTALL_DIR})
install(TARGETS ${DRV_NAME}
	DESTINATION ${INSTALL_DIR})

set(CPACK_GENERATOR "ZIP")
# don't build ALL (but see CMAKE_SKIP_INSTALL_ALL_DEPENDENCY comment)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY TRUE)
set(CPACK_PACKAGE_FILE_NAME
	${DRIVER_BASE_NAME}-${DRV_VERSION}${VERSION_QUALIFIER}-${PLAT_ARCH})
include(CPack)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 :
