# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

add_subdirectory(gtest-1.7.0)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

function(forall_backends target)
	list(REMOVE_AT ARGV 0 )
	foreach(BACKEND IN LISTS backends)
		add_executable( ${target}-${BACKEND} ${ARGV} )
		# Link test executable against gtest & gtest_main
		target_link_libraries(${target}-${BACKEND}
			argo argobackend-${BACKEND} gtest)
		set_target_properties(${target}-${BACKEND} PROPERTIES
			OUTPUT_NAME "${target}"
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${BACKEND}"
			)
		if(${BACKEND} STREQUAL "mpi")
			set(TEST_PARAMETERS mpirun -n ${ARGO_TESTS_NPROCS})
		else()
			set(TEST_PARAMETERS "")
		endif()
		add_test(${target}-${BACKEND}
			${TEST_PARAMETERS} ${CMAKE_BINARY_DIR}/bin/${BACKEND}/${target})
	endforeach(BACKEND)
endfunction(forall_backends)


function(enable_openmp target)
	foreach(BACKEND IN LISTS backends)
		set_target_properties(${target}-${BACKEND} PROPERTIES
			COMPILE_FLAGS "-fopenmp"
			LINK_FLAGS "-fopenmp")
	endforeach(BACKEND)
endfunction(enable_openmp)

################################
# Unit Tests
################################
# Add test cpp file
forall_backends(trivialTests trivial.cpp)
forall_backends(allocatorsTests allocators.cpp)
forall_backends(prefetchTests prefetch.cpp)
forall_backends(ompTests omp.cpp)
forall_backends(cppSTLTests stlallocation.cpp)
forall_backends(barrierTests barrier.cpp)
forall_backends(uninitializedTests uninitialized.cpp)
forall_backends(lockTests lock.cpp)
forall_backends(backendTests backend.cpp)


# Enable OpenMP
enable_openmp(ompTests)

