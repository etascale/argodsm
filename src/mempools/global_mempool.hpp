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
#include "../data_distribution/global_ptr.hpp"

#include <iostream>
#include <memory>
#include <mpi.h>
#include <stdlib.h>
#include <sys/mman.h>

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

				/** @brief dedicated MPI window for the amount of pool memory */
				MPI_Win offset_win;

				/** @brief amount of memory in pool that is already allocated */
				std::ptrdiff_t* offset;
			public:
				/** type of allocation failures within this memory pool */
				using bad_alloc = std::bad_alloc;

				/**
				 * @brief Default constructor: initializes memory on heap and sets offset to 0
				 */
				global_memory_pool() {
					auto nodes = backend::number_of_nodes();
					memory = backend::global_base();
					max_size = backend::global_size();

					MPI_Alloc_mem(sizeof(std::ptrdiff_t), MPI_INFO_NULL, &offset);
					MPI_Win_create(offset, sizeof(std::ptrdiff_t), sizeof(std::ptrdiff_t), MPI_INFO_NULL, MPI_COMM_WORLD, &offset_win);

					/**@todo this initialization should move to tools::init() land */
					using namespace data_distribution;
					base_distribution<0>::set_memory_space(nodes, memory, max_size);

					MPI_Win_lock(MPI_LOCK_EXCLUSIVE, backend::node_id(), 0, offset_win);
					*offset = 0;
					MPI_Win_unlock(backend::node_id(), offset_win);

					backend::barrier();
				}

				/** @todo Documentation */
				~global_memory_pool(){
					MPI_Win_free(&offset_win);

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

					MPI_Win_lock(MPI_LOCK_EXCLUSIVE, backend::node_id(), 0, offset_win);
					*offset = 0;
					MPI_Win_unlock(backend::node_id(), offset_win);

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

					MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, offset_win);
					MPI_Get(offset, 1, MPI_LONG, 0, 0, 1, MPI_LONG, offset_win);
					MPI_Win_flush(0, offset_win); // needed?
					if(*offset+size > max_size) {
						MPI_Win_unlock(0, offset_win);
						throw bad_alloc();
					}
					ptr = &memory[*offset];
					MPI_Accumulate(&size, 1, MPI_LONG, 0, 0, 1, MPI_LONG, MPI_SUM, offset_win);
					MPI_Win_unlock(0, offset_win);

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

					MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, offset_win);
					MPI_Get(offset, 1, MPI_LONG, 0, 0, 1, MPI_LONG, offset_win);
					MPI_Win_flush(0, offset_win); // needed?
					avail = max_size - *offset;
					MPI_Win_unlock(0, offset_win);

					return avail;
				}
		};
	} // namespace mempools
} // namespace argo

#endif /* argo_global_mempool_hpp */
