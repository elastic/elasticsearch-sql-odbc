
# 2.8.6: to generate the _EXPORTS define
# 3.1.0: CMAKE_GENERATOR_PLATFORM
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

set(DRIVER_BASE_NAME elasticodbc CACHE STRING
	"The base name to give the driver")

# driver's version
set(DRV_VER_MAJOR 0)
set(DRV_VER_MINOR 1)

# build a UNICODE driver? (the only supported way currently)
set(IS_UNICODE 1)

#include(GenerateExportHeader)

if (${WIN32})
	if (${CMAKE_GENERATOR_PLATFORM} MATCHES x64)
		#set(CMAKE_GENERATOR_PLATFORM x64)
		set(TARCH x64) # target arch
		set(BARCH ) # bits architecture (64 is the default, silent)
	else (${CMAKE_GENERATOR_PLATFORM} MATCHES x64)
		set(TARCH x86)
		set(BARCH 32)
	endif (${CMAKE_GENERATOR_PLATFORM} MATCHES x64)
	message("Building on Windows, ${TARCH}.")
else (${WIN32})
	message(FATAL_ERROR "No support for current platform yet")
endif (${WIN32})

# explicit languages support (Cs are defaults)
project(${DRIVER_BASE_NAME} CXX C)

if (${IS_UNICODE})
	set(ENCODING u) # Unicode
else (${IS_UNICODE})
	set(ENCODING a) # ANSI
endif (${IS_UNICODE})

# driver name
# XXX: ANSI/Unicode/32/64
set(DRV_NAME "${DRIVER_BASE_NAME}${DRV_VER_MAJOR}${ENCODING}${BARCH}")

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set compiler flags and options.
if (${WIN32})
	# set the Visual Studio warning level to 4
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
	# TODO: review@alpha
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
	# don't complain about non-constant aggregate initializer (C4204)
	# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Ze")

	if (${IS_UNICODE})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DUNICODE /D_UNICODE")
	endif (${IS_UNICODE})
	# set the version
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_VER_MAJOR=${DRV_VER_MAJOR}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_VER_MINOR=${DRV_VER_MINOR}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_ENCODING=${ENCODING}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DDRV_NAME=${DRV_NAME}")
else (${WIN32})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif (${WIN32})
message("C flags: ${CMAKE_C_FLAGS} .")

set(DRV_SRC_DIR driver)
set(DRV_LIB_DIR lib)
message("Directories: source: '${DRV_SRC_DIR}', output: '${DRV_LIB_DIR}'.")

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
	message("Generating module .def file failed:")
	message("\tret: ${CMD_RETURN}")
	message("\tout: ${CMD_OUTPUT}")
	message("\terr: ${CMD_OUTERR}")
endif (${CMD_RETURN})


#
# add ODBC-Specification to the project
#
# TODO: add this as a git submodule and/or as ExternalProject
set(ODBC_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/ODBC-Specification CACHE PATH
	"ODBC-Specification source path")
if (NOT IS_DIRECTORY ${ODBC_PATH_SRC})
	message(FATAL_ERROR "No ODBC-Specification directory found: use "
		"ODBC_PATH_SRC cmake option or place an 'ODBC-Specification' "
		"clone into local 'lib' dir.")
endif (NOT IS_DIRECTORY ${ODBC_PATH_SRC})
message("ODBC-Specification source path: ${ODBC_PATH_SRC} .")
set(ODBC_INC ${ODBC_PATH_SRC}/Windows/inc)


#
# add ujson4c to the project
#
# TODO: add this as a git submodule and/or as ExternalProject (with patching)
set(UJSON4C_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/ujson4c CACHE PATH
	"Lib ujson4c source path")
if (NOT IS_DIRECTORY ${UJSON4C_PATH_SRC})
	message(FATAL_ERROR "No ujson4c directory found: use UJSON4C_PATH_SRC "
		"cmake option or place an 'ujson4c' clone into local 'libs' dir.")
endif (NOT IS_DIRECTORY ${UJSON4C_PATH_SRC})
message("Lib ujson4c source path: ${UJSON4C_PATH_SRC} .")
aux_source_directory(${UJSON4C_PATH_SRC}/src DRV_SRC)
aux_source_directory(${UJSON4C_PATH_SRC}/3rdparty DRV_SRC)
set(UJSON4C_INC ${UJSON4C_PATH_SRC}/src ${UJSON4C_PATH_SRC}/3rdparty )

#
# add c-timestamp to the project
#
# TODO: add this as a git submodule and/or as ExternalProject
set(CTIMESTAMP_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/c-timestamp CACHE PATH
	"Lib c-timestamp source path")
if (NOT IS_DIRECTORY ${CTIMESTAMP_PATH_SRC})
	message(FATAL_ERROR "No c-timestamp directory found: use "
		"CTIMESTAMP_PATH_SRC cmake option or place a 'c-timestamp' clone "
		"in local 'lib' dir.")
endif (NOT IS_DIRECTORY ${CTIMESTAMP_PATH_SRC})
message("Lib c-timestamp source path: ${CTIMESTAMP_PATH_SRC} .")
aux_source_directory(${CTIMESTAMP_PATH_SRC}/ DRV_SRC)

#
# add libcurl to the project
#
# TODO: add this as a git submodule and/or as ExternalProject
set(LIBCURL_LD_PATH
	# curl "installs" the .dll and .lib in different directories -> use the
	# build dir to find both files in same directory instead
	${CMAKE_SOURCE_DIR}/libs/curl/builds/libcurl-vc-${TARCH}-release-dll-ipv6-sspi-winssl-obj-lib/
	CACHE PATH "Lib curl load library path")
set(LIBCURL_INC_PATH ${CMAKE_SOURCE_DIR}/libs/curl/include CACHE PATH
	"Lib curl include path")
if (NOT IS_DIRECTORY ${LIBCURL_LD_PATH}
		OR NOT IS_DIRECTORY ${LIBCURL_INC_PATH})
	message(FATAL_ERROR "Missing libcurl lib and/or inc directories: use "
		"LIBCURL_LD_PATH and LIBCURL_INC_PATH cmake options or place a built "
		"'curl' clone in local 'libs' dir.")
endif()
message("Curl paths load lib: ${LIBCURL_LD_PATH}, inc: ${LIBCURL_INC_PATH} .")
# add libcurl as dependency
add_library(libcurl SHARED IMPORTED)
if (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION
		${LIBCURL_LD_PATH}/libcurl.dll)
	set_property(TARGET libcurl PROPERTY IMPORTED_IMPLIB
		${LIBCURL_LD_PATH}/libcurl.lib)
else (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION
		${LIBCURL_LD_PATH}/libcurl.so)
endif (${WIN32})


message("Driver source files: ${DRV_SRC} .")
message("Driver include paths: " ${ODBC_INC} ${DRV_SRC_DIR}
	${LIBCURL_INC_PATH} ${UJSON4C_INC} ${CTIMESTAMP_PATH_SRC})

#
# finally, set destination library
#
add_library(${DRV_NAME} SHARED ${DRV_SRC} ${CMAKE_BINARY_DIR}/${DRV_NAME}.def)
#generate_export_header(${DRV_NAME})

include_directories(${ODBC_INC} ${DRV_SRC_DIR} ${LIBCURL_INC_PATH}
	${UJSON4C_INC} ${CTIMESTAMP_PATH_SRC})

target_link_libraries(${DRV_NAME} libcurl)

# add testing project/target
enable_testing()
# ... and testing directory to build
add_subdirectory(test)

# add instalation project/target
install(TARGETS ${DRV_NAME} DESTINATION ${DRV_LIB_DIR})


# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 :
