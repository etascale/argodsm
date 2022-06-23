/**
 * @file
 * @brief This file implements the virtual memory and virtual address handling
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C headers
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
// C++ headers
#include <cerrno>
#include <cstddef>
#include <iostream>
#include <string>
#include <system_error>

#include "backend/backend.hpp"
#include "virtual_memory.hpp"

namespace {
/* file constants */
/** @todo hardcoded start address */
char* const ARGO_START = reinterpret_cast<char*>(0x200000000000l);
/** @todo hardcoded size */
const ptrdiff_t ARGO_SIZE = 0x80000000000l;

/** @brief error message string */
const std::string msg_insufficient_memory = "ArgoDSM anonymous mappable memory is insufficient.";
/** @brief error message string */
const std::string msg_mprotect_fail = "ArgoDSM could not mprotect anonymous mapping";
/** @brief error message string */
const std::string msg_invalid_remap = "ArgoDSM encountered an invalid mapping attempt in virtual address space";
/** @brief error message string */
const std::string msg_main_mmap_fail = "ArgoDSM failed to set up virtual memory. Please report a bug.";

/* file variables */
/** @brief the address at which the backing storage space starts */
char* backing_addr;
/** @brief the current offset into the backing storage space */
std::size_t backing_offset;
/** @brief the offset within the memory region controlled to the beginning of the virtual address space */
std::size_t file_offset;
}  // namespace

namespace argo {
namespace virtual_memory {

void init() {
	/** @todo check desired range is free */
	constexpr int flags = MAP_ANONYMOUS|MAP_SHARED|MAP_FIXED|MAP_NORESERVE;
	backing_addr = static_cast<char*>(
		::mmap(static_cast<void*>(ARGO_START), ARGO_SIZE, PROT_NONE, flags, -1, 0));
	if(backing_addr == MAP_FAILED) {
		std::cerr << msg_main_mmap_fail << std::endl;
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_main_mmap_fail);
		exit(EXIT_FAILURE);
	}
	char* virtual_addr = ARGO_START + ARGO_SIZE/2l;
	backing_offset = 0;
	file_offset = virtual_addr - backing_addr;
}

void* start_address() {
	return backing_addr + file_offset;
}

std::size_t size() {
	return ARGO_SIZE/4;
}

void* allocate_mappable(std::size_t alignment, std::size_t size) {
	/* compute next free well-aligned offset */
	backing_offset = ((backing_offset + alignment - 1)/alignment);
	backing_offset *= alignment;

	/* check it is within size limits */
	if(backing_offset > argo::virtual_memory::size()) {
		std::cerr << msg_insufficient_memory << std::endl;
		throw std::system_error(std::make_error_code(std::errc::not_enough_memory), msg_insufficient_memory);
		return nullptr;
	}
	char* r = backing_addr + backing_offset;
	/* mark memory as used */
	backing_offset += size;

	/* make memory writable */
	int err = mprotect(r, size, PROT_READ|PROT_WRITE);
	if(err) {
		std::cerr << msg_mprotect_fail << std::endl;
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_mprotect_fail);
		return nullptr;
	}
	return r;
}

void map_memory(void* addr, std::size_t size, std::size_t offset, int prot) {
	int err = remap_file_pages(addr, size, 0, (file_offset + offset)/PAGE_SIZE, 0);
	if(err) {
		std::cerr << msg_invalid_remap << std::endl;
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_invalid_remap);
		exit(EXIT_FAILURE);
	}
	err = mprotect(addr, size, prot);
	if(err) {
		std::cerr << msg_mprotect_fail << std::endl;
		throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_mprotect_fail);
		exit(EXIT_FAILURE);
	}
}

}  // namespace virtual_memory
}  // namespace argo
