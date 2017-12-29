
# 2.8.6: to generate the _EXPORTS define
cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

#include(GenerateExportHeader)

# explicit languages support (Cs are defaults)
project(elasticodbc CXX C)

# driver's version
# TODO: use it
set(DRV_VER_MAJOR 0)
set(DRV_VER_MINOR 1)

# driver name
# XXX: ANSI/Unicode/32/64
set(DRV_NAME "elasticodbc")

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
 
# Set compiler flags and options. 
# Here it is setting the Visual Studio warning level to 4
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
##set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
# TODO: review@alpha
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D_CRT_SECURE_NO_WARNINGS")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DUNICODE")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
message("C flags:" ${CMAKE_C_FLAGS})

set(DRV_SRC_DIR driver)
set(DRV_LIB_DIR lib)
set(DRV_BUILD_DIR cbuild)
message("Directories: source: '${DRV_SRC_DIR}', output: '${DRV_LIB_DIR}'.")

aux_source_directory(${DRV_SRC_DIR} DRV_SRC)
message("Driver source files: " ${DRV_SRC})

# generate Module definition file (symbols to export)
execute_process(COMMAND 
		${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}/build_def.bat
		${CMAKE_SOURCE_DIR}/${DRV_BUILD_DIR}/${DRV_NAME}.def
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}"
	RESULT_VARIABLE CMD_RETURN
	OUTPUT_VARIABLE CMD_OUTPUT
	ERROR_VARIABLE CMD_OUTERR
	)
#	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/${DRV_SRC_DIR}"
if(${CMD_RETURN})
	message("Generating module .def file failed:")
	message("\tret: ${CMD_RETURN}")
	message("\tout: ${CMD_OUTPUT}")
	message("\terr: ${CMD_OUTERR}")
endif()

add_library(${DRV_NAME} SHARED ${DRV_SRC} ${DRV_BUILD_DIR}/${DRV_NAME}.def)
#generate_export_header(${DRV_NAME})

#include_directories(${DRV_SRC_DIR} ${DRV_BUILD_DIR})
include_directories(${DRV_SRC_DIR})

target_link_libraries(${DRV_NAME}
		)


install(TARGETS ${DRV_NAME}
	DESTINATION ${DRV_LIB_DIR}
)
