/**
 * @file
 * @brief This file implements the virtual memory and virtual address handling
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C headers
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
// C++ headers
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <system_error>

#include "virtual_memory.hpp"

namespace {
/* file constants */
/**
 * @brief The start of the ArgoDSM virtual memory space
 * @note This hard-coded value assumes x86_64 architecture
 *
 * The ArgoDSM virtual memory space leaves the first 1/6 for local use.
 */
char* const ARGO_START = reinterpret_cast<char*>(0x155555554000l);
/**
 * @brief The size of the ArgoDSM virtual memory space
 * @note This hard-coded value assumes x86_64 architecture
 *
 * ArgoDSM reserves half of the available user-space virtual memory.
 * In combination with @ref{ARGO_START}, this ensures that the final third
 * of the virtual memory is left for PIE loads, heap, shared libraries and
 * the stack among other things.
 */
const ptrdiff_t ARGO_SIZE = 0x400000000000l;

/** @brief error message string */
const std::string msg_alloc_fail = "ArgoDSM could not allocate mappable memory";
/** @brief error message string */
const std::string msg_mmap_fail = "ArgoDSM failed to map in virtual address space.";
/** @brief error message string */
const std::string msg_main_mmap_fail = "ArgoDSM failed to set up virtual memory. Please report a bug.";

/* file variables */
/** @brief a file descriptor for backing the virtual address space used by ArgoDSM */
int fd;
/** @brief the address at which the virtual address space used by ArgoDSM starts */
void* start_addr;
}  // namespace

namespace argo {
namespace virtual_memory {

void init() {
	fd = syscall(__NR_memfd_create, "argocache", 0);
	if(ftruncate(fd, ARGO_SIZE)) {
		std::cerr << msg_main_mmap_fail << std::endl;
		/** @todo do something? */
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_main_mmap_fail);
	}
	/** @todo check desired range is free */
	constexpr int flags = MAP_ANONYMOUS|MAP_SHARED|MAP_FIXED;
	start_addr = ::mmap(static_cast<void*>(ARGO_START), ARGO_SIZE, PROT_NONE, flags, -1, 0);
	if(start_addr == MAP_FAILED) {
		std::cerr << msg_main_mmap_fail << std::endl;
		/** @todo do something? */
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_main_mmap_fail);
	}
}

void* start_address() {
	return start_addr;
}

std::size_t size() {
	return ARGO_SIZE/2;
}

void* allocate_mappable(std::size_t alignment, std::size_t size) {
	void* p;
	auto r = posix_memalign(&p, alignment, size);
	if(r || p == nullptr) {
		std::cerr << msg_alloc_fail << std::endl;
		throw std::system_error(std::make_error_code(static_cast<std::errc>(r)), msg_alloc_fail);
		return nullptr;
	}
	return p;
}

void map_memory(void* addr, std::size_t size, std::size_t offset, int prot) {
	auto p = ::mmap(addr, size, prot, MAP_SHARED|MAP_FIXED, fd, offset);
	if(p == MAP_FAILED) {
		std::cerr << msg_mmap_fail << std::endl;
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_mmap_fail);
	}
}

}  // namespace virtual_memory
}  // namespace argo
