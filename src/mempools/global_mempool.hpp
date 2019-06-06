/**
 * @file
 * @brief This file provides a dynamically growing memory pool for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_global_mempool_hpp
#define argo_global_mempool_hpp argo_global_mempool_hpp

/** @todo Documentation */
constexpr int PAGESIZE = 4096;

#include "../backend/backend.hpp"
#include "../synchronization/global_tas_lock.hpp"
#include "../data_distribution/data_distribution.hpp"

#include <sys/mman.h>
#include <memory>
#include <iostream>
#include <stdlib.h>

namespace argo {
	namespace mempools {
		/**
		 * @brief Globalally growing memory pool
		 */
		template<std::size_t chunk_size=4096>
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
				static const std::size_t reserved = 4096;
				/**
				 * @brief Default constructor: initializes memory on heap and sets offset to 0
				 */
				global_memory_pool() {
					auto nodes = backend::number_of_nodes();
					memory = backend::global_base();
					max_size = backend::global_size();
					offset = new (&memory[0]) ptrdiff_t;
					/**@todo this initialization should move to tools::init() land */
					using namespace data_distribution;
					naive_data_distribution<0>::set_memory_space(nodes, memory, max_size);
					bool* flag = new (&memory[sizeof(std::size_t)]) bool;
					global_tas_lock = new argo::globallock::global_tas_lock(flag);

					if(backend::node_id()==0){
						/**@todo if needed - pad offset to be page or pagecache size and make sure offset and flag fits */
						*offset = static_cast<std::ptrdiff_t>(reserved);
					}
					backend::barrier();
				}

				/** @todo Documentation */
				~global_memory_pool(){
					delete global_tas_lock;
					backend::finalize();
				};

				/**
				 *@brief  Resets the memory pool to the initial state instead of de-allocating and (re)allocating all buffers again.
				 *Resets the memory pool to the initial state instead of de-allocating and (re)allocating all buffers again.
				 *Any allocator or memory pool depending on this memory pool now has undefined behaviour.
				 */
				void reset(){
					backend::barrier();
					memory = backend::global_base();
					max_size = backend::global_size();
					if(backend::node_id()==0){
						/**@todo if needed - pad offset to be page or pagecache size and make sure offset and flag fits */
						*offset = static_cast<std::ptrdiff_t>(reserved);
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
				 * @param size minimum size to grow
				 */
				void grow(std::size_t size) {
					(void)size; // size is not needed for unconditional failure
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
	} // namespace mempools
} // namespace argo

#endif /* argo_global_mempool_hpp */
