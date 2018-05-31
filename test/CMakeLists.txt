
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
endif (NOT IS_DIRECTORY ${GTEST_LD_PATH})
message("Googletest paths: load lib: ${GTEST_LD_PATH}, "
	"include: ${GTEST_INC_PATH}.")


include_directories(${CMAKE_SOURCE_DIR}/test ${GTEST_INC_PATH})

# place the build files in test/ dir
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# find all test case source files
file(GLOB TEST_CASES LIST_DIRECTORIES false 
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test_*.cc)
message("Test cases: ${TEST_CASES}")

set(EXTRA_SRC connected_dbc.cc)

# TODO: a better way for this (path resolution [Debug!] & test staging)
# copy DLLs linked (later) against, so that test exes can load them
file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR}/Debug/${DRV_NAME}.dll
	SRC_PATH_ESODBC_DLL)
file(TO_NATIVE_PATH ${LIBCURL_LD_PATH}/libcurl.dll
	SRC_PATH_CURL_DLL)
file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR}/test/Debug/
	DST_PATH_DLL)
add_custom_target(install_shared
	# dir's otherwise created only later on test target execution
	COMMAND if not exist ${DST_PATH_DLL} mkdir ${DST_PATH_DLL}
	COMMAND copy ${SRC_PATH_ESODBC_DLL} ${DST_PATH_DLL}
	COMMAND copy ${SRC_PATH_CURL_DLL} ${DST_PATH_DLL})

foreach (TSRC ${TEST_CASES})
	string(REPLACE ".cxx" "" TBIN ${TSRC})
	add_executable(${TBIN} ${TSRC} ${EXTRA_SRC})
	set_target_properties(${TBIN} PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS})
	target_link_libraries(${TBIN} ${GTEST_LD_PATH}/gtestd.lib
		 ${GTEST_LD_PATH}/gtest_maind.lib)
	target_link_libraries(${TBIN} ${DRV_NAME})
	add_dependencies(${TBIN} install_shared)
	add_test(${TBIN} ${TBIN})
endforeach (TSRC)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 :
