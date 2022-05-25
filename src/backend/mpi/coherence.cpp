/**
 * @file
 * @brief This file implements selective coherence mechanisms for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "../backend.hpp"
#include "swdsm.h"
#include "write_buffer.hpp"
#include "virtual_memory/virtual_memory.hpp"

// EXTERNAL VARIABLES FROM BACKEND
/**
 * @brief This is needed to access page information from the cache
 * @deprecated Should be replaced with a cache API
 */
extern control_data *cacheControl;
/**
 * @brief globalSharers is needed to access and modify the pyxis directory
 * @deprecated Should eventually be handled by a cache module
 */
extern std::uint64_t *globalSharers;
/**
 * @brief A cache mutex protects all operations on cacheControl
 * @deprecated Should eventually be handled by a cache module
 */
extern pthread_mutex_t cachemutex;
/**
 * @brief ibsem is used to serialize all Infiniband (MPI) operations
 * @deprecated Should not be needed once the cache module is implemented
 */
extern sem_t ibsem;
/**
 * @brief sharerWindow protects the pyxis directory
 * @deprecated Should not be needed once the pyxis directory is
 * managed from elsewhere through a cache module.
 */
extern MPI_Win sharerWindow;
/**
 * @brief Needed to update argo statistics
 * @deprecated Should be replaced by API calls to a stats module
 */
extern argo_statistics stats;
/**
 * @brief Needed to update information about cache pages touched
 * @deprecated Should eventually be handled by a cache module
 */
extern argo_byte *touchedcache;
/**
 * @brief workcomm is needed to poke the MPI system during one sided RMA
 */
extern MPI_Comm workcomm;
/**
 * @brief Write buffer to ensure selectively handled pages can be removed
 * @deprecated This should eventually be handled by a cache module
 */
extern write_buffer<std::size_t>* argo_write_buffer;

namespace argo {
	namespace backend {
		void _selective_acquire(void *addr, std::size_t size){
			if(size == 0){
				// Nothing to invalidate
				return;
			}

			const std::size_t block_size = page_size*CACHELINE;
			const std::size_t start_address = reinterpret_cast<std::size_t>(argo::virtual_memory::start_address());
			const std::size_t page_misalignment = reinterpret_cast<std::size_t>(addr)%block_size;
			std::size_t argo_address =
				((reinterpret_cast<std::size_t>(addr)-start_address)/block_size)*block_size;
			const node_id_t node_id = argo::backend::node_id();
			const std::size_t node_id_bit = static_cast<std::size_t>(1) << node_id;

			// Lock relevant mutexes. Start statistics timekeeping
			double t1 = MPI_Wtime();
			pthread_mutex_lock(&cachemutex);
			sem_wait(&ibsem);

			// Iterate over all pages to selectively invalidate
			for(std::size_t page_address = argo_address;
					page_address < argo_address + page_misalignment + size;
					page_address += block_size){
				const node_id_t homenode_id = peek_homenode(page_address);
				// This page should be skipped in the following cases
				// 1. The page is node local so no acquire is necessary
				// 2. The page has not yet been first-touched and trying
				// to perform an acquire would first-touch the page
				if(	homenode_id == node_id ||
					homenode_id == argo::data_distribution::invalid_node_id) {
					continue;
				}

				const std::size_t cache_index = getCacheIndex(page_address);
				const std::size_t classification_index = get_classification_index(page_address);

				// If the page is dirty, downgrade it
				if(cacheControl[cache_index].dirty == DIRTY){
					mprotect((char*)start_address + page_address, block_size, PROT_READ);
					for(int i = 0; i <CACHELINE; i++){
						storepageDIFF(cache_index+i,page_address+page_size*i);
					}
					argo_write_buffer->erase(cache_index);
					cacheControl[cache_index].dirty = CLEAN;
				}

				// Optimization to keep pages in cache if they do not
				// need to be invalidated.
				MPI_Win_lock(MPI_LOCK_SHARED, node_id, 0, sharerWindow);
				if(
						// node is single writer
						(globalSharers[classification_index+1] == node_id_bit)
						||
						// No writer and assert that the node is a sharer
						((globalSharers[classification_index+1] == 0) &&
						 ((globalSharers[classification_index] & node_id_bit) == node_id_bit))
				  ){
					MPI_Win_unlock(node_id, sharerWindow);
					touchedcache[cache_index]=1;
					//nothing - we keep the pages, SD is done in flushWB
				}
				else{ //multiple writer or SO, invalidate the page
					MPI_Win_unlock(node_id, sharerWindow);
					cacheControl[cache_index].dirty=CLEAN;
					cacheControl[cache_index].state = INVALID;
					touchedcache[cache_index]=0;
					mprotect((char*)start_address + page_address, block_size, PROT_NONE);
				}
			}

			double t2 = MPI_Wtime();
			stats.ssitime += t2-t1;

			// Poke the MPI system to force progress
			int flag;
			MPI_Iprobe(MPI_ANY_SOURCE,MPI_ANY_TAG,workcomm,&flag,MPI_STATUS_IGNORE);

			// Release relevant mutexes
			sem_post(&ibsem);
			pthread_mutex_unlock(&cachemutex);
		}

		void _selective_release(void *addr, std::size_t size){
			if(size == 0){
				// Nothing to downgrade
				return;
			}

			const std::size_t block_size = page_size*CACHELINE;
			const std::size_t start_address = reinterpret_cast<std::size_t>(argo::virtual_memory::start_address());
			const std::size_t page_misalignment = reinterpret_cast<std::size_t>(addr)%block_size;
			std::size_t argo_address =
				((reinterpret_cast<std::size_t>(addr)-start_address)/block_size)*block_size;
			const node_id_t node_id = argo::backend::node_id();

			// Lock relevant mutexes. Start statistics timekeeping
			double t1 = MPI_Wtime();
			pthread_mutex_lock(&cachemutex);
			sem_wait(&ibsem);

			// Iterate over all pages to selectively downgrade
			for(std::size_t page_address = argo_address;
					page_address < argo_address + page_misalignment + size;
					page_address += block_size){
				const node_id_t homenode_id = peek_homenode(page_address);
				// selective_release should be skipped in the following cases
				// 1. The page is node local so no release is necessary
				// 2. The page has not yet been first-touched and trying
				// to perform a release would first-touch the page
				if(	homenode_id == node_id ||
					homenode_id == argo::data_distribution::invalid_node_id) {
					continue;
				}
				const std::size_t cache_index = getCacheIndex(page_address);

				// If the page is dirty, downgrade it
				if(cacheControl[cache_index].dirty == DIRTY){
					mprotect((char*)start_address + page_address, block_size, PROT_READ);
					for(int i = 0; i <CACHELINE; i++){
						storepageDIFF(cache_index+i,page_address+page_size*i);
					}
					argo_write_buffer->erase(cache_index);
					cacheControl[cache_index].dirty = CLEAN;
				}
			}

			double t2 = MPI_Wtime();
			stats.ssdtime += t2-t1;

			// Poke the MPI system to force progress
			int flag;
			MPI_Iprobe(MPI_ANY_SOURCE,MPI_ANY_TAG,workcomm,&flag,MPI_STATUS_IGNORE);

			// Release relevant mutexes
			sem_post(&ibsem);
			pthread_mutex_unlock(&cachemutex);
		}
	} //namespace backend
} //namespace argo
