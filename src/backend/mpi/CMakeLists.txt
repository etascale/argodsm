# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

add_library(argobackend-mpi SHARED mpi.cpp swdsm.cpp coherence.cpp)
target_link_libraries(argobackend-mpi qd)

install(TARGETS argobackend-mpi
	COMPONENT "Runtime"
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)

option(ARGO_ENABLE_MT
	"Enable support for multi-threaded MPI in ArgoDSM (MPI_THREAD_MULTIPLE)" OFF)
if(ARGO_ENABLE_MT)
	target_compile_definitions(argobackend-mpi PRIVATE ARGO_ENABLE_MT=1)
else()
	target_compile_definitions(argobackend-mpi PRIVATE ARGO_ENABLE_MT=0)
endif()
