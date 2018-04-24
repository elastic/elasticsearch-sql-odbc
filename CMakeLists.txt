
# 2.8.6: to generate the _EXPORTS define
cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

# driver's version
set(DRV_VER_MAJOR 0)
set(DRV_VER_MINOR 1)
set(BASE_NAME elasticodbc)

# build a UNICODE driver? (currently only supported way)
set(IS_UNICODE 1)

#include(GenerateExportHeader)

# https://cmake.org/cmake/help/latest/variable/CMAKE_GENERATOR_PLATFORM.html :
# "The value of this variable should never be modified by project code."
# , BUT:
# https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2015%202017.html :
# "The CMAKE_GENERATOR_PLATFORM variable may be set to specify a target
# platform name (architecture)."
if (${WIN32})
	if ($ENV{VSCMD_ARG_TGT_ARCH} MATCHES x64)
		set(CMAKE_GENERATOR_PLATFORM x64)
		set(TARCH x64) # target arch
		set(BARCH ) # bits architecture (64 is the default, silent)
	else ($ENV{VSCMD_ARG_TGT_ARCH} MATCHES x64)
		set(TARCH x86)
		set(BARCH 32)
	endif ($ENV{VSCMD_ARG_TGT_ARCH} MATCHES x64)
	message("Building on Windows, ${TARCH}.")
endif (${WIN32})

# explicit languages support (Cs are defaults)
project(${BASE_NAME} CXX C)

if (${IS_UNICODE})
	set(ENCODING u) # Unicode
else (${IS_UNICODE})
	set(ENCODING a) # ANSI
endif (${IS_UNICODE})

# driver name
# XXX: ANSI/Unicode/32/64
set(DRV_NAME "${BASE_NAME}${DRV_VER_MAJOR}${ENCODING}${BARCH}")

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

	#set(LIBCURL_PATH_LIB ${LIBCURL_PATH_LIB}\\libcurl.dll)
else (${WIN32})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

	#set(LIBCURL_PATH_LIB ${LIBCURL_PATH_LIB}/libcurl.so)
endif (${WIN32})
message("C flags: ${CMAKE_C_FLAGS} .")

set(DRV_SRC_DIR driver)
set(DRV_LIB_DIR lib)
set(DRV_BUILD_DIR builds)
message("Directories: source: '${DRV_SRC_DIR}', output: '${DRV_LIB_DIR}'.")

aux_source_directory(${DRV_SRC_DIR} DRV_SRC)

# generate Module definition file (symbols to export)
execute_process(COMMAND 
	${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}/build_def.bat ${DRV_NAME}
		${CMAKE_SOURCE_DIR}/${DRV_BUILD_DIR}/${DRV_NAME}.def
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
if (IS_DIRECTORY $ENV{ODBC_PATH_SRC})
	set(ODBC_PATH_SRC $ENV{ODBC_PATH_SRC})
else (IS_DIRECTORY $ENV{ODBC_PATH_SRC})
	if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/ODBC-Specification)
		set(ODBC_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/ODBC-Specification)
	else (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/ODBC-Specification)
		message(FATAL_ERROR "No ODBC-Specification directory found: set "
				"environment var ODBC_PATH_SRC or place an "
				"'ODBC-Specification' export into local 'lib' dir.")
	endif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/ODBC-Specification)
endif (IS_DIRECTORY $ENV{ODBC_PATH_SRC})
message("ODBC-Specification source path: ${ODBC_PATH_SRC} .")
set(ODBC_INC ${ODBC_PATH_SRC}/Windows/inc )


#
# add ujson4c to the project
#
if (IS_DIRECTORY $ENV{UJSON4C_PATH_SRC})
	set(UJSON4C_PATH_SRC $ENV{UJSON4C_PATH_SRC})
else (IS_DIRECTORY $ENV{UJSON4C_PATH_SRC})
	if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/ujson4c)
		set(UJSON4C_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/ujson4c)
	else (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/ujson4c)
		message(FATAL_ERROR "No ujson4c directory found: set environment var "
				"UJSON4C_PATH_SRC or place an 'ujson4c' export into local "
				"'libs' dir.")
	endif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/ujson4c)
endif (IS_DIRECTORY $ENV{UJSON4C_PATH_SRC})
message("Lib ujson4c source path: ${UJSON4C_PATH_SRC} .")
aux_source_directory(${UJSON4C_PATH_SRC}/src DRV_SRC)
aux_source_directory(${UJSON4C_PATH_SRC}/3rdparty DRV_SRC)
set(UJSON4C_INC ${UJSON4C_PATH_SRC}/src ${UJSON4C_PATH_SRC}/3rdparty )

#
# add libcurl to the project
#
# read libcurl's build paths from the environment (assume local dir otherwise)
if (IS_DIRECTORY $ENV{LIBCURL_PATH_BUILD})
	set(LIBCURL_PATH_BUILD $ENV{LIBCURL_PATH_BUILD})
else (IS_DIRECTORY $ENV{LIBCURL_PATH_BUILD})
	if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/libcurl)
		set(LIBCURL_PATH_BUILD ${CMAKE_SOURCE_DIR}/libs/libcurl)
	else (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/libcurl)
		message(FATAL_ERROR "No libcurl directory found: set environment var "
				"LIBCURL_PATH_BUILD or place a 'libcurl' dir in local source"
				"tree, with 'bin' and 'include' subdirs.")
	endif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/libcurl)
endif (IS_DIRECTORY $ENV{LIBCURL_PATH_BUILD})
message("Lib libcurl build path: ${LIBCURL_PATH_BUILD} .")
# add libcurl as dependency
add_library(libcurl SHARED IMPORTED)
set(LIBCURL_INC ${LIBCURL_PATH_BUILD}/include)
if (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION 
		${LIBCURL_PATH_BUILD}/bin/libcurl.dll)
	set_property(TARGET libcurl PROPERTY IMPORTED_IMPLIB
		${LIBCURL_PATH_BUILD}/lib/libcurl.lib)
else (${WIN32})
	set_property(TARGET libcurl PROPERTY IMPORTED_LOCATION 
		${LIBCURL_PATH_BUILD}/lib/libcurl.so)
endif (${WIN32})

#
# add c-timestamp to the project
#
if (IS_DIRECTORY $ENV{CTIMESTAMP_PATH_SRC})
	set(CTIMESTAMP_PATH_SRC $ENV{CTIMESTAMP_PATH_SRC})
else (IS_DIRECTORY $ENV{CTIMESTAMP_PATH_SRC})
	if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/c-timestamp)
		set(CTIMESTAMP_PATH_SRC ${CMAKE_SOURCE_DIR}/libs/c-timestamp)
	else (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/c-timestamp)
		message(FATAL_ERROR "No c-timestamp directory found: set environment"
				" var CTIMESTAMP_PATH_SRC or place an export into "
				"'c-timestamp' dir in local source tree.")
	endif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/c-timestamp)
endif (IS_DIRECTORY $ENV{CTIMESTAMP_PATH_SRC})
message("Lib c-timestamp source path: ${CTIMESTAMP_PATH_SRC} .")
aux_source_directory(${CTIMESTAMP_PATH_SRC}/ DRV_SRC)
set(CTIMESTAMP_INC ${CTIMESTAMP_PATH_SRC}/ )


message("Driver source files: ${DRV_SRC} .")
message("Driver include paths: " ${ODBC_INC} ${DRV_SRC_DIR} ${LIBCURL_INC} 
	${UJSON4C_INC} ${CTIMESTAMP_INC})

#
# finally, set destination library
#
add_library(${DRV_NAME} SHARED ${DRV_SRC} ${DRV_BUILD_DIR}/${DRV_NAME}.def)
#generate_export_header(${DRV_NAME})

#include_directories(${DRV_SRC_DIR} ${DRV_BUILD_DIR})
include_directories(${ODBC_INC} ${DRV_SRC_DIR} ${LIBCURL_INC} ${UJSON4C_INC} 
	${CTIMESTAMP_INC})


target_link_libraries(${DRV_NAME} libcurl)


install(TARGETS ${DRV_NAME}
	DESTINATION ${DRV_LIB_DIR}
)
