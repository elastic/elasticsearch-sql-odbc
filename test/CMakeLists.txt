#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

# allow the user set the paths, if library already installed on the system
# (not recommended, though)
set(GTEST_LD_PATH "" CACHE PATH "Lib googletest load library path")
set(GTEST_INC_PATH "" CACHE PATH "Lib googletest include path")

if (NOT IS_DIRECTORY ${GTEST_LD_PATH})
	set(GTEST_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/googletest/install)
	include(ExternalProject)
	# build googletest along, tunning it to driver's useage
	ExternalProject_Add(
		googletest
		EXCLUDE_FROM_ALL 1
		#GIT_REPOSITORY "https://github.com/google/googletest.git"
		#GIT_SHALLOW ON
		#GIT_TAG release-1.8.0
		UPDATE_DISCONNECTED ON
		DOWNLOAD_COMMAND git clone --depth 1 https://github.com/google/googletest.git
		UPDATE_COMMAND ""
		CMAKE_ARGS -DBUILD_GMOCK=OFF -DBUILD_GTEST=ON
			-Dgtest_force_shared_crt=ON -DINSTALL_GTEST=ON
			-DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL_PREFIX}
		)

	set(GTEST_LD_PATH ${GTEST_INSTALL_PREFIX}/lib)
	set(GTEST_INC_PATH ${GTEST_INSTALL_PREFIX}/include)
	set(GTEST_LIB ${GTEST_LD_PATH}/gtest${CMAKE_STATIC_LIBRARY_SUFFIX})
	set(GTEST_MAIN_LIB
		${GTEST_LD_PATH}/gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX})
else (NOT IS_DIRECTORY ${GTEST_LD_PATH})
# GoogleTest adds a trailing 'd' to Debug build: find whatever's installed
# (https://github.com/google/googletest/issues/1268)
find_library(GTEST_LIB NAMES gtest gtestd PATHS ${GTEST_LD_PATH})
find_library(GTEST_MAIN_LIB NAMES gtest_main gtest_maind
	PATHS ${GTEST_LD_PATH})
if (${GTEST_LIB_NOTFOUND} or ${GTEST_MAIN_LIB_NOTFOUND})
	message(FATAL_ERROR "Google test libraries not found")
endif (${GTEST_LIB_NOTFOUND})
endif (NOT IS_DIRECTORY ${GTEST_LD_PATH})
message("Googletest paths: load lib: ${GTEST_LD_PATH}, "
	"include: ${GTEST_INC_PATH}.")


include_directories(${CMAKE_SOURCE_DIR}/test ${GTEST_INC_PATH})

# place the build files in test/ dir
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# find all test case source files
file(GLOB TEST_CASES LIST_DIRECTORIES false
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test_*.cc)
message("Driver test cases: ${TEST_CASES}")

set(EXTRA_SRC connected_dbc.cc)
aux_source_directory(${CTIMESTAMP_PATH_SRC}/ CTS_SRC)

# copy DLLs linked (later) against, so that test exes can load them
file(TO_NATIVE_PATH
	${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${DRV_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
	SRC_PATH_ESODBC_DLL)
if (${LIBCURL_LINK_MODE} MATCHES dll)
	file(TO_NATIVE_PATH
		${LIBCURL_LD_PATH}/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX}
		SRC_PATH_CURL_DLL)
endif (${LIBCURL_LINK_MODE} MATCHES dll)
file(TO_NATIVE_PATH
	${DSNBND_LIB_BIN_DIR_BASE}-$<CONFIG>/*${CMAKE_SHARED_LIBRARY_SUFFIX}
	SRC_PATH_DSNBND_DLL)
file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR}/test/${CMAKE_CFG_INTDIR}/
	DST_PATH_DLL)

# gtestd->gtest hack: copy debug libraries to non-debug file names.
# The trouble is, there's no way to configure this behavior (prevening the 'd'
# postfixing) and the libs are built later in the process, not when this code
# is evaluated to generate the project files; which means, no find_library()
# can be triggered here and no way of knowing which build config is going to
# be used either.
file(TO_NATIVE_PATH ${GTEST_INSTALL_PREFIX} GTEST_NATIVE_PREFIX)
# - existing Release googletest libs built along the project (vs. OS provided)
file(TO_NATIVE_PATH ${GTEST_LD_PATH}/gtestd${CMAKE_STATIC_LIBRARY_SUFFIX}
	GTEST_LIBD)
file(TO_NATIVE_PATH ${GTEST_LD_PATH}/gtest_maind${CMAKE_STATIC_LIBRARY_SUFFIX}
	GTEST_MAIND)

add_custom_target(install_shared
	# dir's otherwise created only later on test target execution
	COMMAND if not exist ${DST_PATH_DLL} mkdir ${DST_PATH_DLL}
	COMMAND xcopy /E/Y/F ${SRC_PATH_ESODBC_DLL} ${DST_PATH_DLL}
	COMMAND if exist ${SRC_PATH_CURL_DLL}
		xcopy /E/Y/F ${SRC_PATH_CURL_DLL} ${DST_PATH_DLL}
	COMMAND xcopy /E/Y/F ${SRC_PATH_DSNBND_DLL} ${DST_PATH_DLL}
	# gtest->gtestd hack
	# if googletest just built (i.e. not OS'es) AND the -d version exists
	COMMAND if exist ${GTEST_NATIVE_PREFIX} if exist ${GTEST_LIBD}
		cd ${GTEST_LD_PATH} &&
		copy /Y gtestd${CMAKE_STATIC_LIBRARY_SUFFIX}
			gtest${CMAKE_STATIC_LIBRARY_SUFFIX}
	# (same checks as above)
	COMMAND if exist ${GTEST_NATIVE_PREFIX} if exist ${GTEST_MAIND}
		cd ${GTEST_LD_PATH} &&
			copy /Y gtest_maind${CMAKE_STATIC_LIBRARY_SUFFIX}
				gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}
	)

foreach (TSRC ${TEST_CASES})
	string(REPLACE ".cxx" "" TBIN ${TSRC})

	if (${TBIN} MATCHES ".*test_conversion_c2sql_timestamp.*")
		# this one test requires the c-timestamp lib
		add_executable(${TBIN} ${TSRC} ${EXTRA_SRC} ${CTS_SRC})
	else ()
		add_executable(${TBIN} ${TSRC} ${EXTRA_SRC})
	endif ()

	set_target_properties(${TBIN} PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS})
	target_link_libraries(${TBIN} ${DRV_NAME} ${GTEST_LIB} ${GTEST_MAIN_LIB})
	add_dependencies(${TBIN} install_shared)
	if (GTEST_INSTALL_PREFIX)
		# no pre-existing library on the system -> build gtest(d)
		add_dependencies(${TBIN} googletest)
	endif ()
	add_test(${TBIN} ${TBIN})
endforeach (TSRC)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 :
