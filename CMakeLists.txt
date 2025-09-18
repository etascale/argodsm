# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

cmake_minimum_required(VERSION 3.10)
project(ArgoDSM VERSION 0.1)

set(DEFAULT_C_FLAGS "-std=c17 -pthread -Wall -Wextra -pedantic -Werror")
set(DEFAULT_CXX_FLAGS "-std=c++17 -pthread -DOMPI_SKIP_MPICXX=1 -Wall -Wextra -pedantic -Werror")
set(DEFAULT_LINK_FLAGS "")

option(ARGO_DEBUG
	"Build ArgoDSM without optimization and with debugging symbols" OFF)
if(ARGO_DEBUG)
	set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O0 -g")
	set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} -O0 -g")
	set(DEFAULT_LINK_FLAGS "${DEFAULT_LINK_FLAGS} -g")
else(ARGO_DEBUG)
	set(DEFAULT_C_FLAGS "${DEFAULT_C_FLAGS} -O3 -DNDEBUG")
	set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} -O3 -DNDEBUG")
	set(DEFAULT_LINK_FLAGS "${DEFAULT_LINK_FLAGS} -O3 -DNDEBUG")
endif(ARGO_DEBUG)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${DEFAULT_C_FLAGS}")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${DEFAULT_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${DEFAULT_LINK_FLAGS}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Build with internal RPATH
set(CMAKE_SKIP_BUILD_RPATH  FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Set RPATH for installed libs to point to the lib install dir
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()

# Enabling ARGO_TESTS builds both Googletest and the ArgoDSM tests
option(ARGO_TESTS "Build tests for ArgoDSM" ON)

# Directory containing submodules
# Changing this requires running 'git mv old/submodule new/submodule´ for each submodule
set(SUBMODULES_DIR submodules)

# Function performing submodule update
function(submodule_update submodule)
	message(STATUS "Submodule update: ${SUBMODULES_DIR}/${submodule}")
	execute_process(COMMAND ${GIT_EXECUTABLE} submodule update
		--init --recursive --remote ${SUBMODULES_DIR}/${submodule}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		RESULT_VARIABLE GIT_SUBMOD_RESULT)
	if(NOT GIT_SUBMOD_RESULT EQUAL "0")
		message(FATAL_ERROR "git submodule update --init --recursive "
			"--remote ${SUBMODULES_DIR}/${submodule} failed with ${GIT_SUBMOD_RESULT}, "
			"please checkout submodules")
	else()
		message(STATUS "Submodule update: ${SUBMODULES_DIR}/${submodule} - Success")
	endif()
endfunction()

# Attempt to update submodules from Git
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
	# Update submodules as needed
	option(GIT_SUBMODULE "Check submodules during build" ON)
	if(GIT_SUBMODULE)
		# Update submodules
		submodule_update(qd)
		submodule_update(googletest)
	endif()
endif()

# Check for presence of QD library submodule
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${SUBMODULES_DIR}/qd/CMakeLists.txt")
	message(FATAL_ERROR "The qd submodule was not downloaded! GIT_SUBMODULE "
	"was turned off or failed. Please update submodules and try again.")
else()
	option(QD_TESTS OFF)
	add_subdirectory(${SUBMODULES_DIR}/qd)
	include_directories(${SUBMODULES_DIR}/qd)
endif()

# Add a restricted option to set the ArgoDSM page size
set(VALID_PAGE_SIZES 4096UL)
set(ARGO_PAGE_SIZE 4096UL CACHE STRING "Set the ArgoDSM page size")
set_property(CACHE ARGO_PAGE_SIZE PROPERTY STRINGS ${VALID_PAGE_SIZES})
list(FIND VALID_PAGE_SIZES ${ARGO_PAGE_SIZE} index)
if(index EQUAL -1)
	message(FATAL_ERROR "Invalid ARGO_PAGE_SIZE. Valid options: ${VALID_PAGE_SIZES}")
endif()

# Add a restricted option to set the number of ArgoDSM pages per cache line
set(VALID_CACHE_LINE_SIZES 1L)
set(ARGO_CACHE_LINE_SIZE 1L CACHE STRING "Set the number of ArgoDSM pages per cache line")
set_property(CACHE ARGO_CACHE_LINE_SIZE PROPERTY STRINGS ${VALID_CACHE_LINE_SIZES})
list(FIND VALID_CACHE_LINE_SIZES ${ARGO_CACHE_LINE_SIZE} index)
if(index EQUAL -1)
	message(FATAL_ERROR "Invalid ARGO_CACHE_LINE_SIZE. Valid options: ${VALID_CACHE_LINE_SIZES}")
endif()

# Generate config.hpp containing the above set options and install it
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.hpp.in ${CMAKE_BINARY_DIR}/config/config.hpp @ONLY)
include_directories(${CMAKE_BINARY_DIR}/config)
install(FILES ${CMAKE_BINARY_DIR}/config/config.hpp
	DESTINATION include/argo)

include_directories("${PROJECT_SOURCE_DIR}/src")
add_subdirectory(src)

# Set a default number of MPI processes for make test
set(ARGO_TESTS_NPROCS 2 CACHE STRING "Number of ArgoDSM nodes to execute with make test")
if(NOT ARGO_TESTS_NPROCS MATCHES "^[1-8]$")
	message(FATAL_ERROR "ARGO_TESTS_NPROCS must be a number from 1 to 8.")
endif()

# Check for presence of the Googletests submodule
if(ARGO_TESTS AND NOT EXISTS "${PROJECT_SOURCE_DIR}/${SUBMODULES_DIR}/googletest/CMakeLists.txt")
	message(FATAL_ERROR "The googletest submodule was not downloaded! GIT_SUBMODULE "
	"was turned off or failed. Please update submodules and try again.")
endif()
if(ARGO_TESTS)
	enable_testing()
	# GMock is not used, skip building this
	option(BUILD_GMOCK OFF)

	# Add the googletest dir but exclude it from installation
	include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
	add_subdirectory(${SUBMODULES_DIR}/googletest EXCLUDE_FROM_ALL)

	# Add the tests
	include_directories("${PROJECT_SOURCE_DIR}/tests")
	add_subdirectory(tests)
endif()


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
