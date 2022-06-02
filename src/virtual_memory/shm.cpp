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
#include <cstdlib>
#include <iostream>
#include <string>
#include "virtual_memory.hpp"

namespace {
	/* file constants */
	/** @todo hardcoded start address */
	const char* ARGO_START = reinterpret_cast<char*>(0x200000000000l);
	/** @todo hardcoded end address */
	const char* ARGO_END   = reinterpret_cast<char*>(0x600000000000l);
	/** @todo hardcoded size */
	const ptrdiff_t ARGO_SIZE = ARGO_END - ARGO_START;
	/** @todo hardcoded maximum size */
	const ptrdiff_t ARGO_SIZE_LIMIT = 0x80000000000l;

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
	/** @brief the size of the ArgoDSM virtual address space */
	std::size_t avail;
} // namespace

namespace argo {
	namespace virtual_memory {
		void init() {
			/* find maximum filesize */
			struct statvfs b;
			statvfs("/dev/shm", &b);
			avail = b.f_bavail * b.f_bsize;
			if(avail > static_cast<std::size_t>(ARGO_SIZE_LIMIT)) {
				avail = ARGO_SIZE_LIMIT;
			}
			std::string filename = "/argocache" + std::to_string(getpid());
			fd = shm_open(filename.c_str(), O_RDWR|O_CREAT, 0644);
			if(shm_unlink(filename.c_str())) {
				std::cerr << msg_main_mmap_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_main_mmap_fail);
				exit(EXIT_FAILURE);
			}
			if(ftruncate(fd, avail)) {
				std::cerr << msg_main_mmap_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_main_mmap_fail);
				exit(EXIT_FAILURE);
			}
			/** @todo check desired range is free */
			constexpr int flags = MAP_ANONYMOUS|MAP_SHARED|MAP_FIXED;
			start_addr = ::mmap((void*)ARGO_START, avail, PROT_NONE, flags, -1, 0);
			if(start_addr == MAP_FAILED) {
				std::cerr << msg_main_mmap_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_main_mmap_fail);
				exit(EXIT_FAILURE);
			}
		}

		void* start_address() {
			return start_addr;
		}

		std::size_t size() {
			return avail;
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
				exit(EXIT_FAILURE);
			}
		}
	} // namespace virtual_memory
} // namespace argo
