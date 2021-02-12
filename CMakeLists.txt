# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

cmake_minimum_required(VERSION 2.8)
project(ArgoDSM)
# The version number.
set(ARGO_VERSION_MAJOR 0)
set(ARGO_VERSION_MINOR 1)

set(DEFAULT_C_FLAGS "-std=c11 -pthread")
set(DEFAULT_CXX_FLAGS "-std=c++11 -pthread -DOMPI_SKIP_MPICXX=1")
set(DEFAULT_LINK_FLAGS "")

option(ARGO_DEBUG
	"Build ArgoDSM without optimization and with debugging symbols" OFF)
if(ARGO_DEBUG)
	set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O0 -g -Wall -Wextra -Werror")
	set(DEFAULT_CXX_FLAGS
		"${DEFAULT_CXX_FLAGS} -O0 -g -Wall -Wextra -Werror")
	set(DEFAULT_LINK_FLAGS "${DEFAULT_LINK_FLAGS} -g -Wall -Wextra -Werror")
else(ARGO_DEBUG)
	set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O3")
	set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} -O3")
	set(DEFAULT_LINK_FLAGS "${DEFAULT_LINK_FLAGS} -O3")
endif(ARGO_DEBUG)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${DEFAULT_C_FLAGS}")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${DEFAULT_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${DEFAULT_LINK_FLAGS}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/src")
add_subdirectory(src)

set(ARGO_TESTS_NPROCS 2 CACHE STRING "Number of ArgoDSM nodes to execute with make test")
if(NOT ARGO_TESTS_NPROCS MATCHES "^[1-8]$")
	message(FATAL_ERROR "ARGO_TESTS_NPROCS must be a number from 1 to 8.")
endif()

option(ARGO_TESTS
	"Build tests for ArgoDSM" ON)
if(ARGO_TESTS)
	enable_testing()
	include_directories("${PROJECT_SOURCE_DIR}/tests")
	add_subdirectory(tests)
endif(ARGO_TESTS)

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
	if(NOT DOXYGEN_FOUND)
		message(FATAL_ERROR "Doxygen is needed to build the documentation.")
	endif()

	set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
	set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

	configure_file(${doxyfile_in} ${doxyfile} @ONLY)

	add_custom_target(doc ALL
		COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating API documentation with Doxygen"
		VERBATIM)

	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc)
endif()

