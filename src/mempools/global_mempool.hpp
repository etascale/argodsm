/**
 * @file
 * @brief This file provides a dynamically growing memory pool for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_MEMPOOLS_GLOBAL_MEMPOOL_HPP_
#define ARGODSM_SRC_MEMPOOLS_GLOBAL_MEMPOOL_HPP_

// C headers
#include <stdlib.h>
#include <sys/mman.h>
// C++ headers
#include <iostream>
#include <memory>

#include "backend/backend.hpp"
#include "config.hpp"
#include "data_distribution/global_ptr.hpp"
#include "synchronization/global_tas_lock.hpp"

namespace argo {
namespace mempools {

/**
 * @brief Globally growing memory pool
 */
template<std::size_t chunk_size = PAGE_SIZE>
class global_memory_pool {
	private:
		/** @brief current base address of this memory pool's memory */
		char* memory;

		/** @brief current size of the memory pool */
		std::size_t max_size;

		/** @brief amount of memory in pool that is already allocated */
		std::ptrdiff_t* offset;

		/** @todo Documentation */
		argo::globallock::global_tas_lock *global_tas_lock;

	public:
		/** type of allocation failures within this memory pool */
		using bad_alloc = std::bad_alloc;

		/** reserved space for internal use */
		static const std::size_t reserved = PAGE_SIZE;
		/**
		 * @brief Default constructor: initializes memory on heap and sets offset to 0
		 */
		global_memory_pool() {
			auto nodes = backend::number_of_nodes();
			memory = backend::global_base();
			max_size = backend::global_size();
			/**@todo this initialization should move to tools::init() land */
			using argo::data_distribution::base_distribution;
			base_distribution<0>::set_memory_space(nodes, memory, max_size);

			// Reset maximum size to the full memory size minus the space reserved for internal use
			max_size -= reserved;
			// Attach memory pool offset to the start of the reserved space
			offset = new (&memory[max_size]) ptrdiff_t;

			using tas_lock = argo::globallock::global_tas_lock;
			// Attach internal lock field sizeof(ptrdiff_t) bytes after the start of the reserved space
			tas_lock::internal_field_type* field = new (&memory[max_size+sizeof(std::ptrdiff_t)]) tas_lock::internal_field_type;
			global_tas_lock = new tas_lock(field);

			// Home node makes sure that offset points to Argo's starting address
			using argo::data_distribution::global_ptr;
			global_ptr<char> gptr(&memory[max_size]);
			if(backend::node_id() == gptr.node()) {
				*offset = static_cast<std::ptrdiff_t>(0);
			}
			backend::barrier();
		}

		/** @todo Documentation */
		~global_memory_pool() {
			delete global_tas_lock;
			backend::finalize();
		}

		/**
		 *@brief  Resets the memory pool to the initial state instead of de-allocating and (re)allocating all buffers again.
		 *Resets the memory pool to the initial state instead of de-allocating and (re)allocating all buffers again.
		 *Any allocator or memory pool depending on this memory pool now has undefined behaviour.
		 */
		void reset() {
			backend::barrier();
			memory = backend::global_base();
			// Move back one page as the last page is left for internal use
			max_size = backend::global_size() - reserved;

			// Home node makes sure that offset points to Argo's starting address
			using argo::data_distribution::global_ptr;
			global_ptr<char> gptr(&memory[max_size]);
			if(backend::node_id() == gptr.node()) {
				*offset = static_cast<std::ptrdiff_t>(0);
			}
			backend::barrier();
		}

		/**
		 * @brief Reserve more memory
		 * @param size Amount of memory reserved
		 * @return The pointer to the first byte of the newly reserved memory area
		 * @todo move size check to separate function?
		 */
		char* reserve(std::size_t size) {
			char* ptr;
			global_tas_lock->lock();
			if(*offset+size > max_size) {
				global_tas_lock->unlock();
				throw bad_alloc();
			}
			ptr = &memory[*offset];
			*offset += size;
			global_tas_lock->unlock();
			return ptr;
		}

		/**
		 * @brief fail to grow the memory pool
		 * @param size minimum size to grow (not needed for unconditional failure)
		 */
		void grow(std::size_t size) {
			(void)size;
			throw std::bad_alloc();
		}

		/**
		 * @brief check remaining available memory in pool
		 * @return remaining bytes in memory pool
		 */
		std::size_t available() {
			std::size_t avail;
			global_tas_lock->lock();
			avail = max_size - *offset;
			global_tas_lock->unlock();
			return avail;
		}
};

}  // namespace mempools
}  // namespace argo

#endif  // ARGODSM_SRC_MEMPOOLS_GLOBAL_MEMPOOL_HPP_
