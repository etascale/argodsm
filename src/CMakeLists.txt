# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

include_directories ("${PROJECT_SOURCE_DIR}/backend")
add_subdirectory (backend)

# put together all ArgoDSM source files
set(argo_sources "argo.cpp")

set(allocator_sources allocators.cpp)
foreach(src ${allocator_sources})
	list(APPEND argo_sources allocators/${src})
endforeach(src)

set(env_sources env.cpp)
foreach(src ${env_sources})
	list(APPEND argo_sources env/${src})
endforeach(src)

set(synchronization_sources
	synchronization.cpp
	cohort_lock.cpp
	intranode/mcs_lock.cpp
)
foreach(src ${synchronization_sources})
	list(APPEND argo_sources synchronization/${src})
endforeach(src)


unset(vm_libs)
set(ARGO_VM "SHM" CACHE STRING "Select how to handle virtual addresses")
# Exactly one of these must be enabled (or use the default)
set_property(CACHE ARGO_VM PROPERTY STRINGS ANONYMOUS MEMFD SHM)
if(ARGO_VM STREQUAL "MEMFD")
# "Handle virtual addresses using an anonymous memory file. Requires kernel 3.17+."
	set(vm_sources memfd.cpp)
elseif(ARGO_VM STREQUAL "ANONYMOUS")
# "Handle virtual addresses using anonymously-mapped memory. Slow."
	set(vm_sources anonymous.cpp)
else()  # default #if(ARGO_VM STREQUAL "SHM")
# "Handle virtual addresses using POSIX shared memory. Size-limited."
	set(vm_sources shm.cpp)
	set(vm_libs rt)
endif()

foreach(src ${vm_sources})
	list(APPEND argo_sources virtual_memory/${src})
endforeach(src)

# add the frontend library
add_library(argo SHARED ${argo_sources})

option(ARGO_USE_LIBNUMA
	"Use libnuma to determine NUMA structure within ArgoDSM" ON)
if(ARGO_USE_LIBNUMA)
	target_link_libraries(argo numa)
	add_definitions(-DARGO_USE_LIBNUMA)
endif(ARGO_USE_LIBNUMA)

target_link_libraries(argo ${vm_libs})

#install (TARGETS argo DESTINATION bin)

install(TARGETS argo
	COMPONENT "Runtime"
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)

install(DIRECTORY .
	DESTINATION include
	COMPONENT "Development"
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp")
